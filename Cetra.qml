pragma ComponentBehavior: Bound
import QtQuick
import qs.Commons
import qs.Ui

// Entry point: bar button, panel composition and keyboard lifecycle only.
CetraViewModel {
  id: root
  readonly property var panelHost: root
  i18n: I18n { language: root.preference("locale", "system") }
  property alias keyTarget: keyCatcher
  property alias lightingPaletteToggle: lightingSection.paletteToggle
  property var languageButton: null
  readonly property bool showsPercentage: showPercentage && connected && lowestLevel >= 0 && !button.vertical
  onSettingsExpandedChanged: {
    if (!settingsExpanded) root.focusControl(deviceSettingsToggle)
  }
  onLightingColorExpandedChanged: {
    if (!lightingColorExpanded) root.focusControl(lightingPaletteToggle)
  }
  onLanguageExpandedChanged: {
    if (!languageExpanded) root.focusControl(languageButton)
  }
  onVisibleChanged: { if (!visible) root.close() }

  function collectControls(item, result) {
    if (!item.visible || !item.enabled) return
    if (item.activeFocusOnTab) { result.push(item); return }
    for (var i = 0; i < item.children.length; i++) collectControls(item.children[i], result)
  }
  function moveFocus(direction, tab) {
    if (!opened) return
    var controls = []
    collectControls(column, controls)
    var index = controls.findIndex(function (item) { return item.activeFocus })
    var next = index < 0 ? (direction > 0 ? 0 : controls.length - 1) : index + direction
    if (!controls.length || (tab && (next < 0 || next >= controls.length))) { root.switchPanel(direction); return }
    focusControl(controls[(next + controls.length) % controls.length])
  }
  function focusControl(target) {
    if (!opened || !target || !target.visible || !target.enabled) return
    target.forceActiveFocus()
    var y = target.mapToItem(viewport.contentItem, 0, 0).y
    viewport.contentY = Math.max(0, Math.min(viewport.contentHeight - viewport.height,
      y < viewport.contentY ? y : Math.max(viewport.contentY, y + target.height - viewport.height)))
  }
  function activateFocus() {
    if (!opened) return
    var controls = []
    collectControls(column, controls)
    var target = controls.find(function (item) { return item.activeFocus })
    if (!target) { moveFocus(1, false); return }
    if (typeof target.clicked === "function") target.clicked()
  }
  function handleTextKey(t) {
    if (!opened || !connected) return
    if (["o", "O", "щ", "Щ"].indexOf(t) >= 0) root.setListeningMode("off")
    else if (["n", "N", "т", "Т"].indexOf(t) >= 0) root.setListeningMode("anc")
    else if (["a", "A", "ф", "Ф"].indexOf(t) >= 0) root.setListeningMode("ambient")
    else if (["1", "2", "3"].indexOf(t) >= 0) root.chooseAncLevel(Number(t))
  }

  visible: receiver || !hideWhenReceiverMissing || deviceStatus === "permission-denied" || deviceStatus === "helper-missing" || deviceStatus === "helper-error" || deviceStatus === "starting" || deviceStatus === "waiting"
  implicitWidth: button.implicitWidth
  implicitHeight: button.implicitHeight
  WidgetButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    labelVisible: false
    hasVisualContent: true
    fixedWidth: vertical ? -1 : barContent.implicitWidth + scaledHorizontalMargin * 2
    fixedHeight: vertical ? Style.bar.iconSlot : -1
    horizontalMargin: 7
    tooltipText: root.tr("app.tooltip", "{device}\n{status}\n{battery}\n{left}\n{right}\n{case}\n{microphone}", {
      device: root.tr("app.deviceName", "ROG Cetra SpeedNova"), status: root.statusLabel,
      battery: root.tr("battery.summary", "Last reported: L {left} / R {right} / Case {case}", {
        left: root.levelText(root.leftLevel), right: root.levelText(root.rightLevel), case: root.levelText(root.caseLevel)
      }),
      left: root.tr("report.left", "Left: {report}", { report: root.reportText(root.leftPresent, root.leftCharging, false, true) }),
      right: root.tr("report.right", "Right: {report}", { report: root.reportText(root.rightPresent, root.rightCharging, false, true) }),
      case: root.tr("report.case", "Case: {report}", { report: root.reportText(null, root.caseCharging, true) }),
      microphone: root.tr("microphone.tooltip", "Mic state: unknown / follow headset voice prompt")
    })
    onPressed: function (button) {
      if (button === Qt.RightButton) root.cycleListeningMode()
      else root.toggle()
    }
    onWheelMoved: function (delta) { if (delta !== 0) root.cycleListeningMode() }
    Row {
      id: barContent
      anchors.centerIn: parent
      spacing: root.showMicLevel && button.vertical ? Style.space(2) : Style.space(5)
      CetraIcon { iconSize: Style.bar.iconFont; color: root.barColor; anchors.verticalCenter: parent.verticalCenter }
      MicrophoneLevel {
        root: panelHost
        visible: root.showMicLevel
        anchors.verticalCenter: parent.verticalCenter
      }
      Text {
        textFormat: Text.PlainText
        visible: root.showsPercentage
        text: root.levelText(root.lowestLevel)
        color: root.barColor
        font.family: root.fontFamily
        font.pixelSize: Style.bar.iconFont
        renderType: Text.NativeRendering
        anchors.verticalCenter: parent.verticalCenter
      }
    }
  }
  KeyboardPanel {
    id: panel
    anchorItem: button
    owner: root
    bar: root.bar
    open: root.opened
    focusTarget: keyCatcher
    contentWidth: panel.fittedContentWidth(Style.space(380))
    contentHeight: panel.fittedContentHeight(column.implicitHeight)
    PanelKeyCatcher {
      id: keyCatcher
      LayoutMirroring.enabled: root.i18n.rightToLeft
      LayoutMirroring.childrenInherit: true
      anchors.fill: parent
      onCloseRequested: root.close()
      onTabRequested: function (direction) { root.moveFocus(direction, true) }
      onMoveRequested: function (dx, dy) { root.moveFocus(dx || dy, false) }
      onActivateRequested: root.activateFocus()
      onTextKey: function (t) { root.handleTextKey(t) }
      Flickable {
        id: viewport
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick
        Column {
          id: column
          anchors.left: parent.left
          anchors.right: parent.right
          anchors.top: parent.top
          spacing: Style.spacing.panelGap
          PanelHero {
            width: parent.width
            title: root.tr("app.title", "ROG Cetra")
            meta: root.statusLabel
            foreground: root.foreground
            fontFamily: root.fontFamily
            iconOpacity: root.receiver ? 1.0 : 0.45
            iconComponent: Component { CetraIcon { iconSize: Style.font.display; color: root.foreground } }
            trailingControl: Component {
              ControlButton {
                panelRoot: root
                Component.onCompleted: root.languageButton = this
                Component.onDestruction: if (root.languageButton === this) root.languageButton = null
                label: root.displayLocaleCode
                fontSize: Style.font.caption
                fontFamily: root.fontFamily
                foreground: root.foreground
                accent: root.accent
                bordered: true
                Accessible.name: root.tr("language.title", "INTERFACE LANGUAGE")
                tooltipText: root.languageExpanded ? root.tr("language.collapse", "Interface language  -") : root.tr("language.expand", "Interface language  +")
                onClicked: root.languageExpanded = !root.languageExpanded
              }
            }
          }
          LanguageSection { root: panelHost; width: parent.width }
          BatterySection { root: panelHost; width: parent.width }
          NoiseSection { root: panelHost; width: parent.width }
          MicrophoneSection { root: panelHost; width: parent.width }
          Text {
            textFormat: Text.PlainText
            width: parent.width
            visible: text !== ""
            text: root.settingsFeedback()
            color: root.dim
            font.family: root.fontFamily
            font.pixelSize: Style.font.caption
            wrapMode: Text.WordWrap
          }
          ControlButton {
            panelRoot: root
            id: deviceSettingsToggle
            width: parent.width
            visible: root.connected
            enabled: visible
            label: root.settingsExpanded ? root.tr("settings.collapse", "Device settings  -") : root.tr("settings.expand", "Device settings  +")
            leftAlign: true
            horizontalPadding: 0
            foreground: root.foreground
            accent: root.accent
            fontFamily: root.fontFamily
            fontSize: Style.font.bodySmall
            onClicked: root.settingsExpanded = !root.settingsExpanded
          }
          Column {
            width: parent.width
            spacing: Style.spacing.panelGap
            visible: root.connected && root.settingsExpanded
            enabled: visible
            SettingToggle {
              panelRoot: root
              width: parent.width
              label: root.tr("microphone.showLevel", "Show microphone level")
              value: root.showMicLevel
              onClicked: root.setShowMicLevel(!root.showMicLevel)
            }
            Text {
              textFormat: Text.PlainText
              width: parent.width
              visible: root.showMicLevel
              text: root.tr("microphone.levelHelp", "Measures Cetra input only while another app uses it. Audio is not saved. Silence does not prove mute.")
              color: root.dim
              font.family: root.fontFamily
              font.pixelSize: Style.font.caption
              wrapMode: Text.Wrap
            }
            LightingSection { id: lightingSection; root: panelHost; width: parent.width }
            VoiceSection { root: panelHost; width: parent.width }
          }
        }
      }
    }
  }
}
