pragma ComponentBehavior: Bound
import QtQuick
import qs.Commons
import qs.Ui

FocusScope {
  id: section
  required property var root
  property bool exactExpanded: false
  property bool hexInvalid: false
  property alias toggleControl: lightingPaletteToggle
  property alias feedbackControl: lightingMessage
  implicitHeight: lightingColorColumn.implicitHeight
  Column {
    id: lightingColorColumn
    width: parent.width
    spacing: Style.space(8)
    SettingToggle {
      visible: !root.lightingColorExpanded
      panelRoot: section.root
      width: parent.width
      label: root.tr("lighting.useThemeColor", "Match desktop theme")
      value: root.useThemeColor
      enabled: !root.lightingColorExpanded
      onClicked: root.setLightingSetting("useThemeColor", !root.useThemeColor)
    }
    SettingToggle {
      visible: !root.lightingColorExpanded
      panelRoot: section.root
      width: parent.width
      label: root.tr("lighting.autoTheme", "Update with theme changes")
      value: root.autoThemeColor
      enabled: root.useThemeColor && !root.lightingColorExpanded
      onClicked: root.setAutoThemeColor(!root.autoThemeColor)
    }
    Text {
      textFormat: Text.PlainText
      width: parent.width
      visible: root.autoThemeColor && root.useThemeColor && !root.lightingColorExpanded
      text: root.tr("lighting.autoHelp", "Apply a colored effect once per session. Theme changes then update its color; Off and Cycle stay unchanged.")
      color: root.dim
      font.family: root.fontFamily
      font.pixelSize: Style.font.caption
      wrapMode: Text.Wrap
    }
    ControlButton {
      id: lightingPaletteToggle
      visible: !root.lightingColorExpanded
      panelRoot: section.root
      anchors.horizontalCenter: parent.horizontalCenter
      width: Style.space(48)
      height: width
      radius: width / 2
      label: ""
      foreground: root.foreground
      accent: root.accent
      selected: root.lightingColorExpanded
      Accessible.name: root.tr("lighting.chooseColor", "Choose color")
      tooltipText: root.tr("lighting.chooseColor", "Choose color")
      onClicked: {
        if (root.lightingColorExpanded) root.cancelLightingEdit()
        else root.beginLightingEdit()
      }
      Rectangle {
        anchors.centerIn: parent
        width: Style.space(32)
        height: width
        radius: width / 2
        antialiasing: true
        border.width: 1
        border.color: root.dim
        // RGB data: preview the local draft only while the editor is open.
        color: root.lightingColorExpanded
          ? Qt.rgba(root.lightingDraftRgb[0] / 255, root.lightingDraftRgb[1] / 255, root.lightingDraftRgb[2] / 255, 1)
          : root.selectedLightingColor
            ? Qt.rgba(root.selectedLightingColor.r, root.selectedLightingColor.g, root.selectedLightingColor.b, 1) : "transparent"
      }
    }
    Text {
      textFormat: Text.PlainText
      width: parent.width
      visible: !root.lightingColorExpanded
      text: root.tr("lighting.swatchHint", "Click the circle to choose a color.")
      horizontalAlignment: Text.AlignHCenter
      color: root.dim
      font.family: root.fontFamily
      font.pixelSize: Style.font.caption
      wrapMode: Text.Wrap
    }
    Column {
      width: parent.width
      spacing: Style.space(8)
      visible: root.lightingColorExpanded
      enabled: visible
      onVisibleChanged: {
        section.exactExpanded = false
        section.hexInvalid = false
        hexInput.text = colorField.hexValue
      }
      Row {
        width: parent.width
        spacing: Style.space(8)
        Text {
          textFormat: Text.PlainText
          width: parent.width - effectLabel.implicitWidth - parent.spacing
          text: root.tr("lighting.palette", "Color palette")
          color: root.foreground
          font.family: root.fontFamily
          font.pixelSize: Style.font.caption
          wrapMode: Text.Wrap
        }
        Text {
          textFormat: Text.PlainText
          id: effectLabel
          text: root.lightingText(root.colorApplyEffect)
          color: root.dim
          font.family: root.fontFamily
          font.pixelSize: Style.font.caption
        }
      }
      LightingColorField {
        id: colorField
        root: section.root
        width: parent.width
        onAcceptRequested: root.focusControl(chooseColorButton)
        onEdited: { section.hexInvalid = false; hexInput.text = hexValue }
      }
      Text {
        textFormat: Text.PlainText
        text: root.tr("lighting.brightness", "Brightness")
        color: root.foreground
        font.family: root.fontFamily
        font.pixelSize: Style.font.caption
      }
      PanelSlider {
        id: brightnessSlider
        width: parent.width
        bar: root.bar
        trackColor: root.rule
        fillColor: root.foreground
        knobColor: activeFocus ? root.accent : root.foreground
        value: colorField.brightness
        minimum: 0; maximum: 1; step: 0.01
        activeFocusOnTab: true
        Accessible.role: Accessible.Slider
        Accessible.name: root.tr("lighting.brightness", "Brightness")
        onMoved: function(value) { root.focusControl(brightnessSlider); colorField.setBrightness(value) }
        Keys.onTabPressed: root.moveFocus(1, true)
        Keys.onBacktabPressed: root.moveFocus(-1, true)
        Keys.onPressed: function(event) {
          var delta = event.key === Qt.Key_Right || event.key === Qt.Key_Up ? 0.01 : event.key === Qt.Key_Left || event.key === Qt.Key_Down ? -0.01 : 0
          if (delta === 0) return
          colorField.setBrightness(value + delta)
          event.accepted = true
        }
      }
      ControlButton {
        panelRoot: section.root
        label: section.exactExpanded ? root.tr("lighting.exactColorCollapse", "Exact color  −")
          : root.tr("lighting.exactColorExpand", "Exact color  +")
        fontFamily: root.fontFamily
        fontSize: Style.font.caption
        foreground: root.foreground
        accent: root.accent
        selected: section.exactExpanded
        onClicked: {
          if (section.exactExpanded) { section.hexInvalid = false; hexInput.text = colorField.hexValue }
          section.exactExpanded = !section.exactExpanded
        }
      }
      Column {
        width: parent.width
        spacing: Style.space(6)
        visible: section.exactExpanded
        enabled: visible
        TextField {
          id: hexInput
          width: parent.width
          text: colorField.hexValue
          font.family: root.fontFamily
          font.pixelSize: Style.font.bodySmall
          foreground: root.foreground
          accent: root.accent
          maximumLength: 7
          selectByMouse: true
          activeFocusOnTab: true
          Accessible.name: root.tr("lighting.hexColor", "HEX color")
          onTextEdited: section.hexInvalid = !colorField.setHex(text)
          onAccepted: if (!section.hexInvalid) root.focusControl(chooseColorButton)
          Keys.onTabPressed: root.moveFocus(1, true)
          Keys.onBacktabPressed: root.moveFocus(-1, true)
          // Keep typing (including A/F/digits) local; never invoke ANC shortcuts.
          Keys.priority: Keys.AfterItem
          Keys.onEscapePressed: root.cancelLightingEdit()
          Keys.onPressed: function(event) { event.accepted = true }
        }
        Text {
          textFormat: Text.PlainText
          width: parent.width
          visible: section.hexInvalid
          text: root.tr("lighting.hexInvalid", "Enter six hexadecimal digits (0–9, A–F).")
          color: root.foreground
          font.family: root.fontFamily
          font.pixelSize: Style.font.caption
          wrapMode: Text.Wrap
        }
        Repeater {
          model: ["R", "G", "B"]
          delegate: Row {
            required property string modelData
            required property int index
            width: parent.width
            spacing: Style.space(8)
            Text {
              textFormat: Text.PlainText
              width: Style.space(48)
              anchors.verticalCenter: parent.verticalCenter
              text: root.tr("lighting.channelValue", "{channel}: {value}", {channel: modelData, value: root.lightingDraftRgb[index]})
              color: root.foreground
              font.family: root.fontFamily
              font.pixelSize: Style.font.caption
            }
            PanelSlider {
              id: rgbSlider
              width: parent.width - Style.space(56)
              bar: root.bar
              trackColor: root.rule
              fillColor: root.foreground
              knobColor: activeFocus ? root.accent : root.foreground
              minimum: 0; maximum: 255; step: 1; integer: true
              value: root.lightingDraftRgb[index]
              activeFocusOnTab: true
              Accessible.role: Accessible.Slider
              Accessible.name: modelData
              onMoved: function(value) { root.focusControl(rgbSlider); colorField.setChannel(index, value) }
              Keys.onTabPressed: root.moveFocus(1, true)
              Keys.onBacktabPressed: root.moveFocus(-1, true)
              Keys.onPressed: function(event) {
                var delta = event.key === Qt.Key_Right || event.key === Qt.Key_Up ? 1 : event.key === Qt.Key_Left || event.key === Qt.Key_Down ? -1 : 0
                if (delta === 0) return
                colorField.setChannel(index, Math.max(0, Math.min(255, value + delta)))
                event.accepted = true
              }
            }
          }
        }
      }
      Row {
        id: editorActions
        width: parent.width
        spacing: Style.spacing.controlGap
        ControlButton {
          id: chooseColorButton
          enabled: !section.hexInvalid
          panelRoot: section.root
          width: (editorActions.width - editorActions.spacing) / 2
          label: root.tr("lighting.applyColor", "Apply color")
          foreground: root.foreground
          accent: root.accent
          fontFamily: root.fontFamily
          fontSize: Style.font.bodySmall
          bordered: true
          selected: enabled
          onClicked: root.commitLightingEdit()
        }
        ControlButton {
          panelRoot: section.root
          width: (editorActions.width - editorActions.spacing) / 2
          label: root.lightingFeedback === "saved-not-sent"
            ? root.tr("lighting.closeColor", "Close") : root.tr("lighting.cancelColor", "Cancel")
          foreground: root.foreground
          accent: root.accent
          fontFamily: root.fontFamily
          fontSize: Style.font.bodySmall
          bordered: true
          onClicked: root.cancelLightingEdit()
        }
      }
      Text {
        textFormat: Text.PlainText
        id: lightingMessage
        width: parent.width
        visible: root.lightingDraftError || root.lightingFeedback === "saved-not-sent"
        text: root.lightingDraftError
          ? root.tr("lighting.colorSaveFailed", "Could not save the color. Try again.")
          : root.lightingFeedbackText()
        color: root.dim
        font.family: root.fontFamily
        font.pixelSize: Style.font.caption
        wrapMode: Text.Wrap
        function revealMessage() {
          if (visible) Qt.callLater(function () {
            if (section.root && typeof section.root.scrollControlIntoView === "function")
              section.root.scrollControlIntoView(lightingMessage)
          })
        }
        onVisibleChanged: revealMessage()
        onYChanged: revealMessage()
        onHeightChanged: revealMessage()
      }
    }
    Column {
      width: parent.width
      spacing: Style.space(8)
      visible: !root.lightingColorExpanded
      enabled: visible
      ControlButton {
        panelRoot: section.root
        id: applyColorButton
        visible: root.useThemeColor
        width: parent.width
        label: root.colorApplyEffect === root.lighting
          ? root.tr("lighting.applyColor", "Apply color") : root.tr("lighting.applyStaticColor", "Apply static color")
        foreground: root.foreground
        accent: root.accent
        fontFamily: root.fontFamily
        fontSize: Style.font.bodySmall
        enabled: root.opened && root.settingsExpanded && !root.lightingColorExpanded && root.connected && root.selectedLightingColor !== null
        bordered: true
        onClicked: root.applyLightingColor()
      }
      Text {
        textFormat: Text.PlainText
        width: parent.width
        visible: root.lightingFeedback !== ""
        text: root.lightingFeedbackText()
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
          : root.tr("lighting.colorHelp", "Apply in the palette saves and sends the color.")
        color: root.dim
        font.family: root.fontFamily
        font.pixelSize: Style.font.caption
        wrapMode: Text.WordWrap
      }
    }
  }
}
