pragma ComponentBehavior: Bound
import QtQuick
import qs.Commons
import qs.Ui

FocusScope {
  id: section
  required property var root
  property alias toggleControl: lightingPaletteToggle
  implicitHeight: lightingColorColumn.implicitHeight
  Column {
    id: lightingColorColumn
    width: parent.width
    spacing: Style.space(8)
    ControlButton {
      panelRoot: section.root
      id: lightingPaletteToggle
      width: parent.width
      label: root.lightingColorExpanded ? root.tr("lighting.paletteCollapse", "Color palette  -") : root.tr("lighting.paletteExpand", "Color palette  +")
      foreground: root.foreground
      accent: root.accent
      fontFamily: root.fontFamily
      fontSize: Style.font.bodySmall
      leftAlign: true
      onClicked: root.lightingColorExpanded = !root.lightingColorExpanded
    }
    Column {
      width: parent.width
      spacing: Style.space(8)
      visible: root.lightingColorExpanded
      enabled: visible
      SettingToggle {
        panelRoot: section.root
        width: parent.width
        label: root.tr("lighting.useThemeColor", "Match desktop theme")
        value: root.useThemeColor
        onClicked: root.setLightingSetting("useThemeColor", !root.useThemeColor)
      }
      SettingToggle {
        panelRoot: section.root
        width: parent.width
        label: root.tr("lighting.autoTheme", "Update with theme changes")
        value: root.autoThemeColor
        enabled: root.useThemeColor
        onClicked: root.setAutoThemeColor(!root.autoThemeColor)
      }
      Text {
        textFormat: Text.PlainText
        width: parent.width
        visible: root.autoThemeColor
        text: root.tr("lighting.autoHelp", "Apply a colored effect once per session. Theme changes then update its color; Off and Cycle stay unchanged.")
        color: root.dim
        font.family: root.fontFamily
        font.pixelSize: Style.font.caption
        wrapMode: Text.Wrap
      }
      Rectangle {
        width: parent.width
        height: Style.space(28)
        radius: Style.cornerRadius
        border.width: 1
        border.color: root.rule
        // Device RGB data, not decorative UI color.
        color: root.selectedLightingColor
          ? Qt.rgba(root.selectedLightingColor.r, root.selectedLightingColor.g, root.selectedLightingColor.b, 1) : "transparent"
        Accessible.role: Accessible.StaticText
        Accessible.name: root.selectedLightingColor
          ? root.tr("lighting.colorPreview", "Selected lighting color preview") : root.tr("lighting.invalidColor", "Invalid RGB color")
      }
      Repeater {
        model: [
          { label: root.tr("lighting.red", "Red"), key: "lightingRed" },
          { label: root.tr("lighting.green", "Green"), key: "lightingGreen" },
          { label: root.tr("lighting.blue", "Blue"), key: "lightingBlue" }
        ]
        delegate: Column {
          required property var modelData
          required property int index
          width: parent.width
          visible: !root.useThemeColor
          enabled: visible
          Text {
            textFormat: Text.PlainText
            text: root.tr("lighting.channelValue", "{channel}: {value}", { channel: modelData.label, value: root.lightingRgb[index] })
            color: root.foreground
            font.family: root.fontFamily
            font.pixelSize: Style.font.caption
          }
          PanelSlider {
            id: channelSlider
            width: parent.width
            bar: root.bar
            trackColor: root.rule
            fillColor: root.foreground
            knobColor: activeFocus ? root.accent : root.foreground
            minimum: 0
            maximum: 255
            step: 1
            integer: true
            value: typeof root.lightingRgb[index] === "number" && isFinite(root.lightingRgb[index])
              ? Math.max(0, Math.min(255, Math.round(root.lightingRgb[index]))) : 0
            activeFocusOnTab: true
            Keys.onTabPressed: root.moveFocus(1, true)
            Keys.onBacktabPressed: root.moveFocus(-1, true)
            Accessible.role: Accessible.Slider
            Accessible.name: modelData.label
            Accessible.description: root.tr("lighting.channelHelp", "0 to 255. Arrows change the channel; Enter focuses Apply color.")
            onMoved: root.focusControl(channelSlider)
            onReleased: function (value) { root.setLightingSetting(modelData.key, value) }
            Keys.onPressed: function (event) {
              var delta = event.key === Qt.Key_Right || event.key === Qt.Key_Up ? 1
                : event.key === Qt.Key_Left || event.key === Qt.Key_Down ? -1 : 0
              if (delta !== 0) {
                root.setLightingSetting(modelData.key, Math.max(0, Math.min(255, value + delta)))
                event.accepted = true
              } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                root.focusControl(applyColorButton)
                event.accepted = true
              }
            }
          }
        }
      }
      ControlButton {
        panelRoot: section.root
        id: applyColorButton
        width: parent.width
        label: root.colorApplyEffect === root.lighting
          ? root.tr("lighting.applyColor", "Apply color") : root.tr("lighting.applyStaticColor", "Apply static color")
        foreground: root.foreground
        accent: root.accent
        fontFamily: root.fontFamily
        fontSize: Style.font.bodySmall
        enabled: root.opened && root.settingsExpanded && root.lightingColorExpanded && root.connected && root.selectedLightingColor !== null
        bordered: true
        onClicked: root.applyLightingColor()
      }
      Text {
        textFormat: Text.PlainText
        width: parent.width
        visible: root.lightingFeedback !== ""
        text: root.lightingFeedback === "sent"
          ? root.tr("lighting.requestSent", "Request sent to device helper.")
          : root.tr("lighting.requestRejected", "Not sent. Check the connection and selected color.")
        color: root.dim
        font.family: root.fontFamily
        font.pixelSize: Style.font.caption
        wrapMode: Text.WordWrap
      }
      Text {
        textFormat: Text.PlainText
        width: parent.width
        text: root.selectedLightingColor === null
          ? root.tr("lighting.invalidSettings", "Invalid RGB settings. Choose integer channels from 0 to 255.")
          : root.useThemeColor
            ? root.tr("lighting.themeColorHelp", "Turn off Match desktop theme to choose RGB.")
            : root.tr("lighting.colorHelp", "Choose a color, then apply it.")
        color: root.dim
        font.family: root.fontFamily
        font.pixelSize: Style.font.caption
        wrapMode: Text.WordWrap
      }
    }
  }
}
