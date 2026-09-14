import QtQuick
import qs.Commons
import qs.Ui

// Wrapping host button. panelRoot supplies keyboard routing, not device access.
Button {
  id: controlButton
  required property var panelRoot
  property string label: ""
  text: ""
  implicitWidth: buttonLabel.implicitWidth + horizontalPadding * 2 + Style.normalBorderWidth * 2
  implicitHeight: buttonLabel.implicitHeight + verticalPadding * 2 + Style.normalBorderWidth * 2
  focusable: true
  Keys.forwardTo: [panelRoot.keyTarget]
  Keys.onPressed: function (event) { event.accepted = true }
  Accessible.role: Accessible.Button
  Accessible.name: label
  Text {
    textFormat: Text.PlainText
    id: buttonLabel
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.leftMargin: controlButton.horizontalPadding + Style.normalBorderWidth
    anchors.rightMargin: controlButton.horizontalPadding + Style.normalBorderWidth
    anchors.verticalCenter: parent.verticalCenter
    text: controlButton.label
    color: controlButton.selected ? Style.selectedStateColor(controlButton.foreground, controlButton.accent) : controlButton.foreground
    font.family: controlButton.fontFamily
    font.pixelSize: controlButton.fontSize
    font.bold: controlButton.selected
    horizontalAlignment: controlButton.leftAlign ? Text.AlignLeft : Text.AlignHCenter
    wrapMode: Text.Wrap
  }
}
