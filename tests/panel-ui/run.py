"""Review-only real consumer Qt fixture. No shell, Process, hardware or config writes."""
from pathlib import Path
import json,os,re,select,shutil,subprocess,tempfile
source=Path(__file__).resolve().parent
repo=source.parents[1]
temporary = None if os.environ.get("CETRA_PANEL_EVIDENCE_DIR") else tempfile.TemporaryDirectory(prefix="cetra-panel-", dir="/tmp/opencode")
out=Path(os.environ.get("CETRA_PANEL_EVIDENCE_DIR") or temporary.name).resolve()
out.mkdir(parents=True,exist_ok=True)
work=out/'panel-fixture'
work.mkdir(exist_ok=True)
host=Path('/usr/share/omarchy/shell')
for folder in ('Commons','Ui'):
    shutil.copytree(host/folder,work/'qs'/folder,dirs_exist_ok=True)
for name,boundary in [('Style','  property Process hyprctlProc:'),('Color','  property FileView colorsFile:')]:
    p=work/'qs/Commons'/f'{name}.qml';s=p.read_text();assert boundary in s
    s=s[:s.index(boundary)]+'}\n'
    s=re.sub(r'^import Quickshell(?:\.Io)?\n','',s,flags=re.M)
    s=re.sub(r'Quickshell\.env\("[A-Z_]+"\)','""',s)
    p.write_text(s)
p=work/'qs/Commons/Util.qml';s=p.read_text().replace('import Quickshell\n','')
s=re.sub(r'    Quickshell\.execDetached\([^\n]+','    throw new Error("External execution disabled in review fixture")',s)
p.write_text(s)
for name in ['Panel','KeyboardPanel','WidgetButton']:
    adapter = (source/(name+'.fixture.qml')).read_text()
    if name == 'KeyboardPanel':
        host_panel = (host/'Ui/KeyboardPanel.qml').read_text()
        geometry = host_panel[host_panel.index('  readonly property real screenW:'):host_panel.index('  onOpenChanged:')]
        adapter = adapter.replace('__HOST_GEOMETRY__', geometry)
    (work/'qs/Ui'/(name+'.qml')).write_text(adapter)
plugin=work/'plugin';plugin.mkdir(exist_ok=True)
names=['Cetra','CetraViewModel','CetraIcon','CetraBarIndicator','ControlButton','SettingToggle','LanguageSection','BatterySection','ConnectionSection','BluetoothSoundSection','NoiseSection','MicrophoneSection','LightingSection','LightingPalette','LightingColorField','VoiceSection']
for name in names:
    s=(repo/(name+'.qml')).read_text()
    # Names expose real objects to QtTest without altering geometry/handlers.
    s=re.sub(r'^\s*id: (\w+)',lambda m:m[0]+'; objectName: "'+m[1]+'"',s,flags=re.M)
    (plugin/(name+'.qml')).write_text(s)
shutil.copytree(repo/'assets',plugin/'assets',dirs_exist_ok=True)
s=(repo/'I18n.qml').read_text().replace('import Quickshell.Io\n','')
s=s[:s.index('  property FileView _indexView:')]+'}\n'
catalogs={p.stem:json.loads(p.read_text()) for p in (repo/'locales').glob('*.json') if p.stem!='index'}
registry=json.loads((repo/'locales/index.json').read_text())
s=s.replace('onLocaleChanged: reloadSelection()','onLocaleChanged: fixtureLoad()')
s=s.replace('_ready = true\n    reloadSelection()','fixtureLoad()')
s=s.replace('  function normalizeLocale', '  property var fixtureCatalogs: '+json.dumps(catalogs,ensure_ascii=False)+'\n  function fixtureLoad() {\n    _registry = '+json.dumps(registry,ensure_ascii=False)+'\n    _english = fixtureCatalogs.en\n    _selection = {locale: locale, generation: 1, catalogs: fixtureCatalogs}\n  }\n\n  function normalizeLocale')
(plugin/'I18n.qml').write_text(s)
s=(source/'tst_panel.qml').read_text().replace('__OUTPUT__',str(out))
(work/'tst_panel.qml').write_text(s)
env={**os.environ,'QT_QPA_PLATFORM':'offscreen','QSG_RHI_BACKEND':'opengl','QSG_INFO':'1','QML_DISABLE_DISK_CACHE':'1',
     'QT_QUICK_BACKEND':'rhi','LIBGL_ALWAYS_SOFTWARE':'1','__GLX_VENDOR_LIBRARY_NAME':'mesa',
     '__EGL_VENDOR_LIBRARY_FILENAMES':'/usr/share/glvnd/egl_vendor.d/50_mesa.json'}
xvfb=os.environ.get('CETRA_TEST_XVFB') or shutil.which('Xvfb')
if not xvfb:
    raise SystemExit('Panel checks require Xvfb for private offscreen OpenGL; install xorg-server-xvfb or set CETRA_TEST_XVFB.')
read_fd,write_fd=os.pipe()
try:
    server=subprocess.Popen([xvfb,'-displayfd',str(write_fd),'-screen','0','1024x1024x24','-nolisten','tcp','-noreset'],
                            pass_fds=(write_fd,),stdout=subprocess.DEVNULL,env=env)
finally:
    os.close(write_fd)
try:
    if not select.select([read_fd],[],[],10)[0]:
        raise RuntimeError('Private Xvfb readiness timed out')
    display=os.read(read_fd,32).decode().strip()
    if not display.isdigit():
        raise RuntimeError('Private Xvfb failed before reporting a display')
    env['DISPLAY']=':'+display
    result=subprocess.run(['/usr/lib/qt6/bin/qmltestrunner','-import',str(work),'-input',str(work/'tst_panel.qml')],env=env,timeout=90)
finally:
    os.close(read_fd)
    server.terminate()
    try:
        server.wait(timeout=5)
    except subprocess.TimeoutExpired:
        server.kill()
        server.wait(timeout=5)
    print('Private Xvfb stopped:',server.pid,server.returncode)
raise SystemExit(result.returncode)
