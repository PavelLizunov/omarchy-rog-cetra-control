import QtQuick
import qs.Commons

Rectangle {
  id: section
  required property var root
  visible: root.connected
  activeFocusOnTab: true
  Keys.forwardTo: [root.keyTarget]
  Keys.onPressed: function (event) { event.accepted = true }
  Accessible.role: Accessible.StaticText
  Accessible.name: root.tr("microphone.unknown", "Microphone mute: unknown")
  Accessible.description: root.tr("microphone.tooltip", "Mic state: unknown / follow headset voice prompt")
  border.width: activeFocus ? Style.normalBorderWidth : 0
  border.color: root.accent
  implicitHeight: microphoneState.implicitHeight + Style.spacing.controlPaddingY * 2
  radius: Style.cornerRadius
  color: Style.normalFillFor(root.foreground, root.accent)
  Row {
    id: microphoneState
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.margins: Style.space(12)
    anchors.verticalCenter: parent.verticalCenter
    spacing: Style.space(12)
    CetraIcon {
      name: "cetra"
      color: root.dim
      iconSize: Style.font.display
      anchors.verticalCenter: parent.verticalCenter
    }
    Column {
      width: parent.width - parent.children[0].width - parent.spacing
      spacing: Style.space(3)
      Text {
        textFormat: Text.PlainText
        text: root.tr("microphone.unknown", "Microphone mute: unknown")
        color: root.foreground
        font.family: root.fontFamily
        font.pixelSize: Style.font.bodySmall
        font.bold: true
        width: parent.width
        wrapMode: Text.WordWrap
      }
      Text {
        textFormat: Text.PlainText
        width: parent.width
        visible: root.showMicLevel
        text: root.opened && root.showMicLevel ? root.microphoneLevelText() : ""
        color: root.dim
        font.family: root.fontFamily
        font.pixelSize: Style.font.caption
        wrapMode: Text.WordWrap
      }
      Text {
        textFormat: Text.PlainText
        text: root.callContextActive
          ? root.tr("microphone.callGesture", "Call mode requested. Follow the headset voice prompt; tap behavior is not confirmed.")
          : root.tr("microphone.mediaGesture", "Call mode not requested. A tap may control playback.")
        color: root.dim
        font.family: root.fontFamily
        font.pixelSize: Style.font.caption
        width: parent.width
        wrapMode: Text.WordWrap
      }
    }
  }
}
