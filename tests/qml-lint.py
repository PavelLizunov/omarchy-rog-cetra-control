"""Resolve Quickshell's qs imports in an isolated copy, without repository links."""
import collections
import json
import pathlib
import shutil
import subprocess
import tempfile
import re

root = pathlib.Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix="cetra-qml-", dir="/tmp/opencode") as tmp:
    imports = pathlib.Path(tmp)
    for name in ("Commons", "Ui"):
        shutil.copytree(pathlib.Path("/usr/share/omarchy/shell") / name, imports / "qs" / name)
    # Lexical access to explicit section root/modelData properties is intentional
    # in this codebase. Report style advice as info; semantic warnings stay fatal.
    cmd = ["/usr/lib/qt6/bin/qmllint", "--json", "-", "-W", "0", "--unqualified", "info",
           "-I", "/usr/lib/qt6/qml", "-I", str(imports)]
    inputs = sorted(root.glob("*.qml"))
    result = subprocess.run(cmd + [str(p) for p in inputs], capture_output=True, text=True, timeout=30)
    report = json.loads(result.stdout)
    if {pathlib.Path(f['filename']) for f in report.get('files', [])} != set(inputs):
        raise SystemExit('QML lint report did not cover exactly the requested files')
    if result.returncode not in (0, 255):
        raise SystemExit(f'QML lint tool failed: {result.returncode}: {result.stderr}')
    # These declarations are intentionally QtObject-typed in the installed host.
    # Verify concrete members in their owning sources instead of inventing types
    # or suppressing all missing-property diagnostics.
    style = pathlib.Path('/usr/share/omarchy/shell/Commons/Style.qml').read_text()
    colors = pathlib.Path('/usr/share/omarchy/shell/Commons/Color.qml').read_text()
    bar_api = pathlib.Path('/usr/share/omarchy/shell/plugins/bar/Bar.qml').read_text()
    dynamic = 0
    errors = 0
    host_limitations = 0
    for file in report["files"]:
        path = pathlib.Path(file['filename'])
        lines = path.read_text().splitlines()
        counts = collections.Counter()
        for warning in file["warnings"]:
            category = warning['id']
            if warning['type'] == 'info': continue
            line = lines[warning['line'] - 1]
            member = re.fullmatch(r'Member "(\w+)" not found on type "QObject"', warning['message'])
            verified = False
            if member:
                name = member[1]
                token = re.search(r'Style\.(font|spacing|bar)\.' + name + r'\b', line)
                surface = re.search(r'Color\.(popups|bar)\.' + name + r'\b', line)
                if token:
                    block = re.search(r'property QtObject ' + token[1] + r': QtObject \{(.*?)^  \}', style, re.M | re.S)
                    verified = bool(block and re.search(r'property \w+ ' + name + r'\s*:', block[1]))
                elif surface:
                    block = re.search(r'property QtObject ' + surface[1] + r': QtObject \{(.*?)^  \}', colors, re.M | re.S)
                    verified = bool(block and re.search(r'property \w+ ' + name + r'\s*:', block[1]))
                elif path.name == 'CetraViewModel.qml' and name in ('shell','foreground','accent','urgent','fontFamily'):
                    verified = bool(re.search(r'property \w+ ' + name + r'\s*:', bar_api))
            if verified:
                dynamic += 1
                continue
            if (path.name in ('CetraService.qml', 'CetraPreferences.qml') and category == 'signal-handler-parameters'
                    and warning['message'].startswith('Type QProcess::ExitStatus ')
                    and line.strip() in ('onExited: root.watcherStopped()', 'onExited: function (exitCode) {')):
                # Quickshell 0.3.1 qmltypes references a non-exported Qt enum.
                # Neither handler uses that enum; the settings reader uses exitCode.
                host_limitations += 1
                continue
            counts[category] += 1
            errors += 1
            print(f'{path.name}:{warning["line"]}: {category}: {warning["message"]}')
        print(path.name, dict(counts))
    print(f'Host dynamic members verified: {dynamic}; unused host signal type exceptions: {host_limitations}; unresolved semantic diagnostics: {errors}')
    print(result.stderr)
    raise SystemExit(1 if errors else 0)
