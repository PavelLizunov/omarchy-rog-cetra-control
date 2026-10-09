pragma ComponentBehavior: Bound
import QtQuick
import qs.Commons
import qs.Ui

Column {
  id: section
  required property var root
  spacing: Style.space(8)
  visible: root.languageExpanded
  enabled: visible
  PanelSectionHeader {
    width: parent.width
    text: root.tr("language.title", "INTERFACE LANGUAGE")
    foreground: root.foreground
    fontFamily: root.fontFamily
  }
  Grid {
    id: languageGrid
    width: parent.width
    columns: 3
    spacing: Style.space(6)
    Repeater {
      model: root.languageOptions
      delegate: ControlButton {
        panelRoot: section.root
        required property var modelData
        width: Math.floor((languageGrid.width - languageGrid.spacing * 2) / 3)
        label: modelData.label
        foreground: root.foreground
        accent: root.accent
        fontFamily: root.fontFamily
        fontSize: Style.font.caption
        bordered: false
        active: root.currentLocaleCode === modelData.code
        onClicked: {
          // Retranslation rebuilds the Repeater; close before saving.
          var code = modelData.code
          var owner = section.root
          owner.languageExpanded = false
          owner.languageSaveFailed = !owner.setLocaleSetting(code)
          if (owner.languageSaveFailed) {
            owner.languageExpanded = true
            owner.languageSaveFailed = true
          }
        }
      }
    }
  }
  Text {
    textFormat: Text.PlainText
    id: languageError
    width: parent.width
    visible: root.languageSaveFailed
    text: root.tr("language.saveFailed", "Could not save the language. Try again.")
    color: root.dim
    font.family: root.fontFamily
    font.pixelSize: Style.font.caption
    wrapMode: Text.Wrap
  }
}
