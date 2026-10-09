pragma ComponentBehavior: Bound
import QtQuick
import qs.Commons
import qs.Ui

Column {
  id: section
  required property var root
  property alias paletteToggle: palette.toggleControl
  property alias feedbackControl: palette.feedbackControl
  spacing: Style.space(8)
  visible: root.connected
  PanelSectionHeader {
    visible: !root.lightingColorExpanded
    width: parent.width
    text: root.tr("lighting.title", "LIGHTING")
    foreground: root.foreground
    fontFamily: root.fontFamily
  }
  Row {
    id: lightingRow1
    visible: !root.lightingColorExpanded
    width: parent.width
    spacing: Style.space(6)
    Repeater {
      model: ["off", "cycle"]
      delegate: ControlButton {
        panelRoot: section.root
        enabled: !root.lightingColorExpanded
        required property string modelData
        width: (lightingRow1.width - lightingRow1.spacing) / 2
        label: root.lightingText(modelData)
        foreground: root.foreground
        fontFamily: root.fontFamily
        fontSize: Style.font.bodySmall
        bordered: false
        active: root.lighting === modelData
        onClicked: root.setLighting(modelData)
      }
    }
  }
  Row {
    id: lightingRow2
    visible: !root.lightingColorExpanded
    width: parent.width
    spacing: Style.space(6)
    Repeater {
      model: [
        { label: root.lightingText("static"), value: "static" },
        { label: root.lightingText("breathing"), value: "breathing" },
        { label: root.lightingText("strobing"), value: "strobing" }
      ]
      delegate: ControlButton {
        panelRoot: section.root
        enabled: !root.lightingColorExpanded
        required property var modelData
        width: (lightingRow2.width - lightingRow2.spacing * 2) / 3
        label: modelData.label
        foreground: root.foreground
        fontFamily: root.fontFamily
        fontSize: Style.font.caption
        bordered: false
        active: root.lighting === modelData.value
        onClicked: root.setLighting(modelData.value)
      }
    }
  }
  Text {
    textFormat: Text.PlainText
    width: parent.width
    visible: !root.lightingColorExpanded && root.lighting !== "unknown"
    text: root.tr("lighting.lastSent", "Last sent: {effect}", { effect: root.lightingText(root.lighting) })
    color: root.dim
    font.family: root.fontFamily
    font.pixelSize: Style.font.caption
    wrapMode: Text.WordWrap
  }
  LightingPalette {
    id: palette
    width: parent.width
    root: section.root
  }
}
