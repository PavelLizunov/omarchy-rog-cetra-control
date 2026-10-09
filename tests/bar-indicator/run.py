"""Render the actual bar component in isolated Qt; no audio/HID or shell instance."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile

repo=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='cetra-bar-') as temporary:
    fixture=Path(temporary)
    commons=fixture/'imports/qs/Commons'
    commons.mkdir(parents=True)
    (commons/'qmldir').write_text('module qs.Commons\nsingleton Style 1.0 Style.qml\n')
    (commons/'Style.qml').write_text('''pragma Singleton
import QtQuick
QtObject { property QtObject bar: QtObject { property real iconFont: 13 } }
''')
    source_dir=Path(os.environ.get('CETRA_BAR_SOURCE_DIR',str(repo)))
    shutil.copy2(source_dir/'CetraBarIndicator.qml',fixture/'CetraBarIndicator.qml')
    (fixture/'Preview.qml').write_text('''import QtQuick
import qs.Commons
Rectangle {
  width: 760; height: 535; color: "black"
  Repeater {
    id: cases
    model: [
      {label:"0% / 0%", l:0, r:0, mic:0, show:true},
      {label:"100% / 100%", l:100, r:100, mic:0, show:true},
      {label:"0% / 100%", l:0, r:100, mic:0, show:true},
      {label:"Charge unavailable", l:null, r:null, mic:0, show:true},
      {label:"Invalid charge", l:-1, r:"50", mic:0, show:true},
      {label:"50% / silence", l:50, r:50, mic:0, show:true},
      {label:"50% / speech", l:50, r:50, mic:0.8, show:true},
      {label:"50% / no capture", l:50, r:50, mic:null, show:true},
      {label:"50% / meter hidden", l:50, r:50, mic:0.8, show:false},
      {label:"50% / quiet speech", l:50, r:50, mic:0.3, show:true},
      {label:"Theme color", l:50, r:50, mic:0.3, show:true, tint:"#50e0b0"},
      {label:"25% / 75%", l:25, r:75, mic:0.3, show:true}
    ]
    delegate: Item {
      id: cell
      required property int index
      required property var modelData
      property alias bar: barItem
      width:190; height:150; x:(index%4)*190; y:Math.floor(index/4)*150
      QtObject {
        id: model
        property var leftLevel: cell.modelData.l
        property var rightLevel: cell.modelData.r
        property var leftPresent: true
        property var rightPresent: true
        property var microphoneLevel: cell.modelData.mic
        property bool connected: true
        property string microphoneCaptureState: "active"
        property bool showMicLevel: cell.modelData.show
        property color barColor: cell.modelData.tint || "white"
        property color barForeground: "white"
        function microphoneLevelText() { return "Fixture microphone" }
      }
      Text { x:8; y:8; text:cell.modelData.label; color:"white"; font.pixelSize:12 }
      CetraBarIndicator { id:barItem; root:model; x:40; y:30; scale:4; transformOrigin:Item.TopLeft }
    }
  }
  Text { x:8; y:460; text:"Actual-size rendering: 0 / 25 / 50 / 100 percent"; color:"white"; font.pixelSize:12 }
  Repeater {
    model: [0,25,50,100]
    delegate: Item {
      id: small
      required property int index
      required property var modelData
      x:40+index*190; y:490
      QtObject {
        id: smallModel
        property var leftLevel: small.modelData
        property var rightLevel: small.modelData
        property var leftPresent: true
        property var rightPresent: true
        property var microphoneLevel: 0.8
        property bool connected: true
        property string microphoneCaptureState: "active"
        property bool showMicLevel: true
        property color barColor: "white"
        property color barForeground: "white"
        function microphoneLevelText() { return "Fixture microphone" }
      }
      CetraBarIndicator { root:smallModel }
    }
  }
  function check() {
    for (var i=0;i<cases.count;i++) {
      var b=cases.itemAt(i).bar
      if (b.implicitWidth!==27 || b.implicitHeight!==26) { console.error("Geometry",i); return false }
    }
    var probe=cases.itemAt(0).bar
    for (var invalid of [null, undefined, NaN, Infinity, -1, 101, "50", {}]) {
      if (probe.isValidCharge(invalid)) { console.error("Invalid charge accepted"); return false }
    }
    for (var valid of [0,25,50,100]) if (!probe.isValidCharge(valid)) return false
    var signal=cases.itemAt(6).bar
    if (!signal.isMicSignalValid) return false
    signal.root.connected=false
    if (signal.isMicSignalValid || signal.micFillVisible) return false
    signal.root.connected=true
    signal.root.microphoneCaptureState="inactive"
    if (signal.isMicSignalValid || signal.micFillVisible) return false
    signal.root.microphoneCaptureState="active"
    Style.bar.iconFont=26
    var scaled=probe.implicitWidth===54 && probe.implicitHeight===52
    Style.bar.iconFont=13
    if (!scaled) { console.error("Theme scaling"); return false }
    return true
  }
}
''')
    flags=subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Quick','Qt6Gui','Qt6Qml'],text=True).split()
    exe=fixture/'render'
    subprocess.run(['c++','-O2','-Wall','-Wextra','-o',str(exe),str(Path(__file__).with_name('render.cpp')),*flags],check=True)
    env=dict(os.environ,QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software')
    backend=os.environ.get('CETRA_BAR_BACKEND','software')
    if backend == 'opengl':
        env.pop('QT_QUICK_BACKEND',None)
        env['CETRA_BAR_FORCE_OPENGL']='1'
    elif backend != 'software':
        raise ValueError('CETRA_BAR_BACKEND must be software or opengl')
    output=fixture/'preview.png'
    subprocess.run([str(exe),str(fixture),str(output)],env=env,check=True,timeout=20)
    # An explicit evidence directory retains this one test image; ordinary suite runs leave no artifacts.
    if os.environ.get('CETRA_BAR_EVIDENCE_DIR'):
        destination=Path(os.environ['CETRA_BAR_EVIDENCE_DIR'])
        destination.mkdir(parents=True,exist_ok=True)
        shutil.copy2(output,destination/'qt-bar-preview.png')
        for name in ('actual-size.png','pixel-zoom.png'):
            shutil.copy2(fixture/name,destination/name)
    source=(repo/'Cetra.qml').read_text()
    assert re.search(r'CetraBarIndicator\s*\{[^}]*root: panelHost',source)
    assert 'if (button === Qt.RightButton) root.cycleListeningMode()' in source
    assert 'else root.toggle()' in source
    assert 'if (delta !== 0) root.cycleListeningMode()' in source
    print('PASS bar consumer: shared view model, left/right/wheel wiring preserved')
