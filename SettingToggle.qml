import QtQuick
import qs.Commons
import qs.Ui

// One pointer owner; unknown values remain distinct from confirmed Off.
FocusScope {
  id: toggleRow
  required property var panelRoot
  property string label: ""
  property var value: null
  signal clicked()
  implicitHeight: Math.max(Style.spacing.controlHeight, toggleLabel.implicitHeight + Style.space(16))
  activeFocusOnTab: true
  Keys.forwardTo: [panelRoot.keyTarget]
  Keys.onPressed: function (event) { event.accepted = true }
  Accessible.role: Accessible.CheckBox
  Accessible.name: label
  Accessible.description: panelRoot.settingStateText(value)
  Accessible.checked: value === true
  Accessible.onToggleAction: if (visible && enabled) clicked()
  Rectangle {
    anchors.fill: parent
    radius: Style.cornerRadius
    color: toggleRow.activeFocus ? Style.focusFillFor(toggleRow.panelRoot.foreground, toggleRow.panelRoot.accent)
      : toggleMouse.containsMouse ? Style.hoverFillFor(toggleRow.panelRoot.foreground, toggleRow.panelRoot.accent) : "transparent"
    border.width: toggleRow.activeFocus ? Style.normalBorderWidth : 0
    border.color: toggleRow.panelRoot.accent
  }
  Text {
    textFormat: Text.PlainText
    id: toggleLabel
    anchors.left: parent.left
    anchors.right: toggleSwitch.left
    anchors.margins: Style.space(12)
    anchors.verticalCenter: parent.verticalCenter
    text: toggleRow.panelRoot.tr("settings.toggleState", "{label} / {state}", { label: toggleRow.label, state: toggleRow.panelRoot.settingStateText(toggleRow.value) })
    color: toggleRow.panelRoot.foreground
    font.family: toggleRow.panelRoot.fontFamily
    font.pixelSize: Style.font.caption
    wrapMode: Text.Wrap
  }
  ToggleSwitch {
    id: toggleSwitch
    anchors.right: parent.right
    anchors.rightMargin: Style.space(10)
    anchors.verticalCenter: parent.verticalCenter
    checked: toggleRow.value === true
    interactive: false
    Accessible.ignored: true
  }
  MouseArea {
    id: toggleMouse
    anchors.fill: parent
    hoverEnabled: true
    cursorShape: Qt.PointingHandCursor
    onClicked: {
      toggleRow.panelRoot.focusControl(toggleRow)
      toggleRow.clicked()
    }
  }
}
