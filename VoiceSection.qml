pragma ComponentBehavior: Bound
import QtQuick
import qs.Commons
import qs.Ui

Column {
  id: section
  required property var root
  spacing: Style.space(8)
  visible: root.connected
  PanelSectionHeader {
    width: parent.width
    text: root.tr("voice.title", "VOICE PROMPTS")
    foreground: root.foreground
    fontFamily: root.fontFamily
  }
  Row {
    id: voicePromptRow
    width: parent.width
    spacing: Style.space(6)
    Repeater {
      model: [
        { label: root.tr("voice.english", "English"), value: "english" },
        { label: root.tr("voice.chinese", "Chinese"), value: "chinese" },
        { label: root.tr("voice.beeps", "Beeps"), value: "sound" }
      ]
      delegate: ControlButton {
        panelRoot: section.root
        required property var modelData
        width: (voicePromptRow.width - voicePromptRow.spacing * 2) / 3
        label: modelData.label
        foreground: root.foreground
        fontFamily: root.fontFamily
        fontSize: Style.font.caption
        bordered: false
        active: root.voicePrompt === modelData.value
        enabled: root.pendingSettings.voice_prompt === undefined
        onClicked: root.setVoicePrompt(modelData.value)
      }
    }
  }
  Text {
    textFormat: Text.PlainText
    visible: root.voicePrompt === "unknown"
    text: root.tr("voice.unknown", "Voice prompt / Unknown")
    color: root.dim
    font.family: root.fontFamily
    font.pixelSize: Style.font.caption
  }
}
