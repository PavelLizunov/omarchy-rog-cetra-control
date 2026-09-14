pragma ComponentBehavior: Bound
import QtQuick
import qs.Commons
import qs.Ui

Column {
  id: section
  required property var root
  spacing: Style.space(10)
  visible: root.connected
  PanelSectionHeader {
    width: parent.width
    text: root.tr("noise.title", "NOISE CONTROL")
    foreground: root.foreground
    fontFamily: root.fontFamily
  }
  Row {
    id: modeRow
    width: parent.width
    spacing: 0
    Repeater {
      model: root.modeOptions
      delegate: ControlButton {
        panelRoot: section.root
        required property var modelData
        width: (modeRow.width - modeRow.spacing * 2) / 3
        label: modelData.label
        foreground: root.foreground
        accent: root.accent
        fontFamily: root.fontFamily
        fontSize: Style.font.bodySmall
        selected: root.listeningMode === modelData.value
        enabled: root.pendingMode === ""
        tooltipText: root.tr("noise.shortcut", "{shortcut} / {mode}", { shortcut: modelData.shortcut, mode: modelData.label })
        onClicked: root.setListeningMode(modelData.value)
        Rectangle {
          anchors.left: parent.left
          anchors.right: parent.right
          anchors.bottom: parent.bottom
          height: Style.space(2)
          color: root.listeningMode === modelData.value ? root.foreground : root.rule
        }
      }
    }
  }
  Column {
    width: parent.width
    spacing: Style.space(6)
    visible: root.listeningMode === "anc"
    Row {
      id: ancLevelRow
      width: parent.width
      spacing: Style.space(6)
      Repeater {
        model: [
          { label: root.tr("noise.low", "Low"), value: 1 },
          { label: root.tr("noise.mid", "Mid"), value: 2 },
          { label: root.tr("noise.high", "High"), value: 3 }
        ]
        delegate: ControlButton {
          panelRoot: section.root
          required property var modelData
          width: (ancLevelRow.width - ancLevelRow.spacing * 2) / 3
          label: modelData.label
          foreground: root.foreground
          fontFamily: root.fontFamily
          fontSize: Style.font.caption
          bordered: false
          active: root.ancLevel === modelData.value && root.ancAdaptive === false
          enabled: root.pendingSettings.anc_level === undefined && root.pendingSettings.anc_adaptive === undefined
          onClicked: root.chooseAncLevel(modelData.value)
        }
      }
    }
    SettingToggle {
      panelRoot: section.root
      width: parent.width
      label: root.tr("noise.adaptive", "Adaptive ANC")
      value: root.ancAdaptive
      enabled: root.pendingSettings.anc_adaptive === undefined
      onClicked: root.setAncAdaptive(root.ancAdaptive !== true)
    }
    Text {
      textFormat: Text.PlainText
      visible: root.ancLevel === null
      text: root.tr("noise.levelUnknown", "ANC level / Unknown")
      color: root.dim
      font.family: root.fontFamily
      font.pixelSize: Style.font.caption
    }
  }
  Text {
    textFormat: Text.PlainText
    width: parent.width
    text: root.pendingMode !== ""
      ? root.tr("noise.switching", "Switching to {mode}\u2026", { mode: root.modeText(root.pendingMode) })
      : root.modeRequestTimedOut ? root.tr("noise.notConfirmed", "Mode change not confirmed. Try again.")
      : root.tr("noise.shortcuts", "O / {off}    N / {anc}    A / {ambient}", {
        off: root.modeText("off"), anc: root.modeText("anc"), ambient: root.modeText("ambient")
      })
    color: root.dim
    font.family: root.fontFamily
    font.pixelSize: Style.font.caption
    wrapMode: Text.WordWrap
  }
}
