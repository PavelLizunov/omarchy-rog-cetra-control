import QtQuick
import qs.Commons
Item {
 property QtObject bar: null
 property string moduleName: ""
 property bool manageIpc: false
 property var settings: ({})
 property bool opened: false
 readonly property color barForeground: bar ? bar.barForeground : Color.foreground
 function open() { opened=true }
 function close() { opened=false }
 function toggle() { opened=!opened }
 function switchPanel(direction) { return false }
}
