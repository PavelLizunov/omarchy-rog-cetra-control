pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as Controls
import qs.Commons
import qs.Ui
import "." as Plugin

// Entry point: bar button, panel composition and keyboard lifecycle only.
CetraViewModel {
  id: root
  readonly property var panelHost: root
  i18n: Plugin.I18n { language: root.preference("locale", "system") }
  property alias keyTarget: keyCatcher
  property alias lightingPaletteToggle: lightingSection.paletteToggle
  property var languageButton: null
  onLightingColorExpandedChanged: {
    if (!lightingColorExpanded) Qt.callLater(function () {
      if (root && typeof root.focusControl === "function") root.focusControl(root.lightingPaletteToggle)
    })
  }
  onLanguageExpandedChanged: {
    if (languageExpanded) {
      root.languageSaveFailed = false
      root.cancelLightingEdit()
      viewport.contentY = 0
    }
    else Qt.callLater(function () {
      if (root && typeof root.focusControl === "function") root.focusControl(root.languageButton)
    })
  }
  onAudioIdentityChanged: {
    if (opened && !bluetoothExpanded) Qt.callLater(root.focusCurrentTab)
  }
  onBluetoothExpandedChanged: {
    if (!bluetoothExpanded && opened) Qt.callLater(function () {
      if (root && typeof root.focusControl === "function") root.focusControl(root.panelPage === "device" ? root.connectionDetailsButton : soundTab)
    })
  }
  onConnectedChanged: {
    if (connected) root.bluetoothExpanded = false
    else root.cancelLightingEdit()
    if (opened) Qt.callLater(root.focusCurrentTab)
  }
  onBluetoothAudioConnectedChanged: {
    if (opened) Qt.callLater(root.focusCurrentTab)
  }
  onVisibleChanged: { if (!visible) root.close() }

  function focusCurrentTab() {
    if (!root || typeof root.focusControl !== "function") return
    root.focusControl(root.panelPage === "device" ? deviceTab : soundTab)
  }
  function showPage(page) {
    if (page !== "sound" && page !== "device") return
    if (page !== "device") root.cancelLightingEdit()
    root.languageExpanded = false
    root.panelPage = page
    Qt.callLater(function () {
      if (!root || typeof root.focusCurrentTab !== "function") return
      root.focusCurrentTab()
      viewport.contentY = 0
    })
  }

  function showDevicePage(page) {
    if (["settings", "color"].indexOf(page) < 0) return
    root.cancelLightingEdit()
    root.devicePage = page
    Qt.callLater(function () {
      if (!root || typeof root.focusControl !== "function") return
      root.focusControl(root.devicePage === "color" ? colorTab : deviceSettingsTab)
      viewport.contentY = 0
    })
  }

  function dismissPanelLayer() {
    if (root.lightingColorExpanded) root.cancelLightingEdit()
    else if (root.languageExpanded) root.languageExpanded = false
    else root.close()
  }

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
    scrollControlIntoView(target)
  }
  function scrollControlIntoView(target) {
    if (!opened || !target || !target.visible) return
    // Header and tabs do not belong to the scrolling page.
    var ancestor = target
    while (ancestor && ancestor !== viewport.contentItem) ancestor = ancestor.parent
    if (!ancestor) return
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

  visible: panelAvailable || bluetoothCandidates.length > 0 || !hideWhenDisconnected || deviceStatus === "permission-denied" || deviceStatus === "helper-missing" || deviceStatus === "helper-error" || deviceStatus === "starting" || deviceStatus === "waiting"
  implicitWidth: button.implicitWidth
  implicitHeight: button.implicitHeight
  WidgetButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    labelVisible: false
    hasVisualContent: true
    fixedWidth: vertical ? -1 : Math.max(Style.bar.iconSlot, barContent.implicitWidth)
    fixedHeight: vertical ? Math.max(Style.bar.iconSlot, barContent.implicitHeight) : -1
    horizontalMargin: 0
    verticalPadding: 0
    tooltipText: root.tr("app.tooltip", "{device}\n{status}\n{battery}", {
      device: root.tr("app.deviceName", "ROG Cetra SpeedNova"), status: root.statusLabel,
      battery: root.tr("battery.summary", "Last reported: L {left} / R {right} / Case {case}", {
        left: root.levelText(root.leftLevel), right: root.levelText(root.rightLevel), case: root.levelText(root.caseLevel)
      })
    })
    onPressed: function (button) {
      if (button === Qt.RightButton) root.cycleListeningMode()
      else root.toggle()
    }
    onWheelMoved: function (delta) { if (delta !== 0) root.cycleListeningMode() }
    CetraBarIndicator {
      id: barContent
      anchors.centerIn: parent
      root: panelHost
    }
  }
  KeyboardPanel {
    id: panel
    anchorItem: button
    owner: root
    bar: root.bar
    open: root.opened
    focusTarget: keyCatcher
    readonly property real preferredWidth: Style.space(380)
    readonly property real preferredHeight: Style.space(560)
    contentWidth: panel.fittedContentWidth(preferredWidth)
    contentHeight: panel.cappedContentHeight(preferredHeight)
    PanelKeyCatcher {
      id: keyCatcher
      LayoutMirroring.enabled: root.i18n.rightToLeft
      LayoutMirroring.childrenInherit: true
      anchors.fill: parent
      onCloseRequested: root.dismissPanelLayer()
      onTabRequested: function (direction) { root.moveFocus(direction, true) }
      onMoveRequested: function (dx, dy) { root.moveFocus(dx || dy, false) }
      onActivateRequested: root.activateFocus()
      onTextKey: function (t) { root.handleTextKey(t) }
      Item {
        id: column
        anchors.fill: parent
        Column {
          id: navigation
          anchors.top: parent.top
          width: parent.width
          spacing: Style.spacing.panelGap
          PanelHero {
            width: parent.width
            title: root.tr("app.title", "ROG Cetra")
            meta: root.connected ? root.tr("connection.usbHeader", "Connected via USB")
              : root.bluetoothAudioConnected === true ? root.tr("connection.bluetoothConnected", "Connected via Bluetooth") : root.statusLabel
            foreground: root.foreground
            fontFamily: root.fontFamily
            iconOpacity: root.panelAvailable ? 1.0 : 0.45
            iconComponent: Component { CetraIcon { iconSize: Style.font.display; color: root.foreground } }
            trailingControl: Component {
              Button {
                id: headerLanguageButton
                Component.onCompleted: root.languageButton = this
                Component.onDestruction: if (root.languageButton === this) root.languageButton = null
                text: root.displayLocaleCode
                iconText: "文"
                iconSize: Style.font.bodySmall
                fontSize: Style.font.caption
                fontFamily: root.fontFamily
                foreground: root.foreground
                accent: root.accent
                bordered: true
                focusable: true
                selected: root.languageExpanded
                Keys.forwardTo: [root.keyTarget]
                Accessible.role: Accessible.Button
                Accessible.name: root.tr("language.title", "INTERFACE LANGUAGE")
                Accessible.onPressAction: headerLanguageButton.clicked()
                tooltipText: root.tr("language.current", "Interface language: {language}", { language: root.displayLocaleCode })
                onClicked: root.languageExpanded = !root.languageExpanded
              }
            }
          }
          Row {
            id: pageTabs
            width: parent.width
            spacing: Style.spacing.controlGap
            ControlButton {
              id: soundTab
              height: Math.max(Style.space(36), implicitHeight)
              panelRoot: root
              width: (pageTabs.width - pageTabs.spacing) / 2
              label: root.tr("panel.sound", "Sound")
              selected: root.panelPage === "sound"
              bordered: true
              foreground: root.foreground
              accent: root.accent
              fontFamily: root.fontFamily
              fontSize: Style.font.bodySmall
              onClicked: root.showPage("sound")
            }
            ControlButton {
              id: deviceTab
              height: Math.max(Style.space(36), implicitHeight)
              panelRoot: root
              width: (pageTabs.width - pageTabs.spacing) / 2
              label: root.tr("panel.device", "Device")
              selected: root.panelPage === "device"
              bordered: true
              foreground: root.foreground
              accent: root.accent
              fontFamily: root.fontFamily
              fontSize: Style.font.bodySmall
              onClicked: root.showPage("device")
            }
          }
          // Reserve the sub-tab row on every page so content keeps its anchor.
          Item {
            width: parent.width
            height: deviceTabs.implicitHeight
            Row {
              id: deviceTabs
              width: parent.width
              visible: root.panelPage === "device" && !root.languageExpanded
              enabled: visible
              spacing: Style.spacing.controlGap
              ControlButton {
                id: deviceSettingsTab
                height: Math.max(Style.space(28), implicitHeight)
                panelRoot: root
                horizontalPadding: Style.space(10)
                verticalPadding: Style.space(4)
                label: root.tr("panel.settings", "Settings")
                selected: root.devicePage === "settings"
                foreground: root.foreground
                accent: root.accent
                fontFamily: root.fontFamily
                fontSize: Style.font.caption
                bordered: false
                onClicked: root.showDevicePage("settings")
              }
              ControlButton {
                id: colorTab
                height: Math.max(Style.space(28), implicitHeight)
                panelRoot: root
                horizontalPadding: Style.space(10)
                verticalPadding: Style.space(4)
                label: root.tr("panel.color", "Color")
                selected: root.devicePage === "color"
                foreground: root.foreground
                accent: root.accent
                fontFamily: root.fontFamily
                fontSize: Style.font.caption
                bordered: false
                onClicked: root.showDevicePage("color")
              }
            }
          }
        }
        Flickable {
          id: viewport
          anchors.top: navigation.bottom
          anchors.topMargin: Style.spacing.panelGap
          anchors.left: parent.left
          anchors.right: parent.right
          anchors.bottom: parent.bottom
          // Reserve the scrollbar gutter even when the page is short.
          anchors.rightMargin: Style.spacing.controlGap
          contentWidth: width
          contentHeight: pageContent.implicitHeight
          onContentHeightChanged: {
            if (root.lightingColorExpanded && lightingSection.feedbackControl.visible)
              root.scrollControlIntoView(lightingSection.feedbackControl)
          }
          clip: true
          boundsBehavior: Flickable.StopAtBounds
          flickableDirection: Flickable.VerticalFlick
          Controls.ScrollBar.vertical: Controls.ScrollBar {
            id: scrollBar
            parent: column
            anchors.top: viewport.top
            anchors.bottom: viewport.bottom
            anchors.right: parent.right
            width: Style.space(3)
            policy: Controls.ScrollBar.AsNeeded
            contentItem: Rectangle {
              implicitWidth: Style.space(3)
              radius: width / 2
              color: root.dim
              opacity: scrollBar.active || scrollBar.hovered ? 0.8 : 0.4
            }
          }
          Column {
            id: pageContent
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: Style.spacing.panelGap
            LanguageSection { root: panelHost; width: parent.width }
            Column {
              width: parent.width
              spacing: Style.spacing.panelGap
              visible: root.panelPage === "sound" && !root.languageExpanded
              enabled: visible
              BatterySection { root: panelHost; width: parent.width }
              NoiseSection { root: panelHost; width: parent.width }
              BluetoothSoundSection { root: panelHost; width: parent.width }
              ControlButton {
                panelRoot: root
                width: parent.width
                visible: !root.connected && root.audioIdentity !== "selected" && root.bluetoothCandidates.length > 0
                label: root.tr("connection.selectEarbuds", "Select Bluetooth earbuds")
                foreground: root.foreground
                accent: root.accent
                fontFamily: root.fontFamily
                bordered: true
                onClicked: {
                  root.showPage("device")
                  root.showDevicePage("settings")
                }
              }
              MicrophoneSection { root: panelHost; width: parent.width }
            }
            Column {
              width: parent.width
              spacing: Style.spacing.panelGap
              visible: root.panelPage === "device" && !root.languageExpanded
              enabled: visible
              Column {
                width: parent.width
                spacing: Style.spacing.panelGap
                visible: root.devicePage === "settings"
                enabled: visible
                SettingToggle {
                  panelRoot: root
                  width: parent.width
                  label: root.tr("microphone.showLevel", "Show microphone level")
                  value: root.showMicLevel
                  onClicked: root.setShowMicLevel(!root.showMicLevel)
                }
                ConnectionSection { root: panelHost; width: parent.width }
                Column {
                  width: parent.width
                  spacing: Style.spacing.panelGap
                  visible: root.connected
                  enabled: visible
                  VoiceSection { root: panelHost; width: parent.width }
                  Text {
                    textFormat: Text.PlainText
                    width: parent.width
                    visible: text !== ""
                    text: root.settingsFeedback(["voice_prompt"])
                    color: root.dim
                    font.family: root.fontFamily
                    font.pixelSize: Style.font.caption
                    wrapMode: Text.WordWrap
                  }
                  Text {
                    textFormat: Text.PlainText
                    width: parent.width
                    text: root.tr("noise.shortcuts", "O / {off}    N / {anc}    A / {ambient}", {
                      off: root.modeText("off"), anc: root.modeText("anc"), ambient: root.modeText("ambient")
                    })
                    color: root.dim
                    font.family: root.fontFamily
                    font.pixelSize: Style.font.caption
                    wrapMode: Text.WordWrap
                  }
                }
              }
              Column {
                width: parent.width
                spacing: Style.spacing.panelGap
                visible: root.devicePage === "color"
                enabled: visible
                Text {
                  textFormat: Text.PlainText
                  width: parent.width
                  visible: !root.connected
                  text: root.tr("lighting.usbOnly", "Connect the earbuds through USB to control lighting.")
                  color: root.dim
                  font.family: root.fontFamily
                  font.pixelSize: Style.font.caption
                  wrapMode: Text.Wrap
                }
                LightingSection { id: lightingSection; root: panelHost; width: parent.width }
              }
            }
          }
        }
      }
    }
  }
}
