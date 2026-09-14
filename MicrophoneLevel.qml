import QtQuick
import qs.Commons

// One compact amplitude bar. Null is an unavailable mark, never a mute claim.
Item {
  id: meter
  required property var root
  property color foreground: root.barForeground
  readonly property var level: root.microphoneLevel
  implicitWidth: Style.space(3)
  implicitHeight: Style.bar.iconFont
  Accessible.role: Accessible.StaticText
  Accessible.name: root.microphoneLevelText()
  Rectangle {
    anchors.fill: parent
    color: Qt.rgba(meter.foreground.r, meter.foreground.g, meter.foreground.b, 0.16)
    Rectangle {
      anchors.bottom: parent.bottom
      width: parent.width
      height: meter.level === null ? Style.space(1) : parent.height * Math.max(0, Math.min(1, meter.level))
      color: meter.foreground
      opacity: meter.level === null ? 0.4 : 1
    }
  }
}
