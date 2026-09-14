pragma ComponentBehavior: Bound
import QtQuick
import qs.Commons

Column {
  id: section
  required property var root
  spacing: Style.spacing.panelGap
  Row {
    id: batteryRow
    width: parent.width
    spacing: Style.spacing.panelGap
    Repeater {
      model: [
        { label: root.tr("battery.left", "LEFT"), value: root.leftLevel, present: root.leftPresent, charging: root.leftCharging, isCase: false, icon: "cetra-left" },
        { label: root.tr("battery.right", "RIGHT"), value: root.rightLevel, present: root.rightPresent, charging: root.rightCharging, isCase: false, icon: "cetra-right" },
        { label: root.tr("battery.case", "CASE"), value: root.caseLevel, present: null, charging: root.caseCharging, isCase: true, icon: "cetra-case" }
      ]
      delegate: Column {
        required property var modelData
        width: (batteryRow.width - batteryRow.spacing * 2) / 3
        spacing: Style.spacing.labelGap
        CetraIcon {
          anchors.horizontalCenter: parent.horizontalCenter
          name: modelData.icon
          iconSize: Style.font.display
          color: root.foreground
          opacity: modelData.value === null ? 0.4 : 1
        }
        Text {
          textFormat: Text.PlainText
          width: parent.width
          text: modelData.label
          horizontalAlignment: Text.AlignHCenter
          color: root.dim
          font.family: root.fontFamily
          font.pixelSize: Style.font.caption
          font.letterSpacing: 1
        }
        Text {
          textFormat: Text.PlainText
          width: parent.width
          text: root.levelText(modelData.value)
          horizontalAlignment: Text.AlignHCenter
          color: modelData.value !== null && Number(modelData.value) <= 20 ? (root.bar ? root.bar.urgent : Color.urgent) : root.foreground
          font.family: root.fontFamily
          font.pixelSize: modelData.value === null ? Style.font.bodySmall : Style.font.heading
          font.bold: modelData.value !== null
        }
        Text {
          textFormat: Text.PlainText
          width: parent.width
          visible: text !== ""
          text: root.batteryStatusText(modelData.present, modelData.charging, modelData.isCase)
          horizontalAlignment: Text.AlignHCenter
          color: root.dim
          font.family: root.fontFamily
          font.pixelSize: Style.font.caption
          wrapMode: Text.WordWrap
        }
        Rectangle {
          width: parent.width
          height: Style.space(2)
          color: root.rule
          Rectangle {
            height: parent.height
            width: parent.width * Math.max(0, Math.min(100, Number(modelData.value || 0))) / 100
            color: modelData.value !== null && Number(modelData.value) <= 20 ? (root.bar ? root.bar.urgent : Color.urgent) : root.foreground
            Behavior on width { NumberAnimation { duration: 150 } }
          }
        }
      }
    }
  }
  Text {
    textFormat: Text.PlainText
    width: parent.width
    text: root.tr("battery.lastReported", "Battery values are last reported.")
    color: root.dim
    font.family: root.fontFamily
    font.pixelSize: Style.font.caption
    wrapMode: Text.WordWrap
  }
}
