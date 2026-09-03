import QtQuick
import Quickshell
import Quickshell.Io
import qs.Commons
import qs.Ui

Panel {
  id: root
  moduleName: "io.github.pavellizunov.rog-cetra-control"
  manageIpc: false

  readonly property string watchCommand: Qt.resolvedUrl("bin/cetra-watch").toString().replace("file://", "")
  readonly property color foreground: bar ? bar.foreground : Color.foreground
  readonly property color accent: bar ? bar.accent : Color.accent
  readonly property color barColor: lowestLevel >= 0 && lowestLevel <= 20
    ? (bar ? bar.urgent : Color.urgent)
    : barForeground
  readonly property color dim: Qt.rgba(foreground.r, foreground.g, foreground.b, 0.58)
  readonly property string fontFamily: bar ? bar.fontFamily : Style.font.family

  readonly property bool showPercentage: setting("showPercentage", true) === true
  readonly property bool hideWhenReceiverMissing: setting("hideWhenReceiverMissing", true) === true

  property string deviceStatus: "starting"
  property bool receiver: false
  property bool connected: false
  property var leftLevel: null
  property var rightLevel: null
  property var caseLevel: null
  property string listeningMode: "unknown"
  property string pendingMode: ""
  property int ancLevel: 3
  property bool ancAdaptive: true
  property string voicePrompt: "english"
  property bool proximity: false
  property string lighting: "off"
  property int tapSeq: 0
  property bool micLive: true
  property bool callContextActive: false
  property bool requestedCallContextActive: false
  property bool detectedCallContext: false
  property bool alwaysCallContext: setting("alwaysCallContext", false) === true
  property int inactiveCallPolls: 0

  readonly property int lowestLevel: {
    var levels = []
    if (leftLevel !== null) levels.push(Number(leftLevel))
    if (rightLevel !== null) levels.push(Number(rightLevel))
    return levels.length ? Math.min.apply(null, levels) : -1
  }

  readonly property bool showsPercentage: showPercentage && connected && lowestLevel >= 0 && !button.vertical
  readonly property var modeOptions: [
    { value: "off", label: "Off", icon: "󰟢", shortcut: "O" },
    { value: "anc", label: "ANC", icon: "󰞀", shortcut: "N" },
    { value: "ambient", label: "Ambient", icon: "󰖟", shortcut: "A" }
  ]
  readonly property string statusLabel: {
    if (deviceStatus === "helper-missing") return "Device helper is not installed"
    if (deviceStatus === "permission-denied") return "No permission to read the receiver"
    if (deviceStatus === "protocol-error") return "Unsupported receiver response"
    if (deviceStatus === "timeout" || deviceStatus === "busy") return "Waiting for receiver data"
    if (!receiver) return "USB receiver is not connected"
    if (!connected) return "Earbuds are in the case"
    return "Connected through USB receiver"
  }

  function levelText(value) {
    return value === null || value === undefined ? "In case" : Number(value) + "%"
  }

  function applyStatus(text) {
    var data
    try {
      data = JSON.parse(String(text || "").trim())
    } catch (error) {
      return
    }

    var status = String(data.status || "unknown")
    receiver = data.receiver === true
    if (status !== "ok") {
      deviceStatus = status
      return
    }

    deviceStatus = "ok"
    receiver = data.receiver === true
    connected = data.connected === true
    leftLevel = data.left === undefined ? null : data.left
    rightLevel = data.right === undefined ? null : data.right
    caseLevel = data.case === undefined ? null : data.case
    listeningMode = String(data.mode || "unknown")
    if (data.anc_level !== undefined) ancLevel = Number(data.anc_level)
    if (data.anc_adaptive !== undefined) ancAdaptive = data.anc_adaptive === true
    if (data.voice_prompt !== undefined) voicePrompt = String(data.voice_prompt)
    if (data.proximity !== undefined) proximity = data.proximity === true
    if (data.lighting !== undefined) lighting = String(data.lighting)
    if (data.mic_live !== undefined) micLive = data.mic_live === true
    callContextActive = data.call_context === true
    if (pendingMode !== "" && listeningMode === pendingMode) {
      pendingMode = ""
      modeRequestTimeout.stop()
    }
  }

  function applyDeviceState(text) {
    try {
      var data = JSON.parse(String(text || "").trim())
      if (data.status === "ok") applyStatus(text)
    } catch (error) {
    }
  }

  function setListeningMode(mode) {
    if (!connected || pendingMode !== "") return
    pendingMode = mode
    modeRequestTimeout.restart()
    deviceWatchProc.write("mode " + mode + "\n")
  }

  function setAncLevel(level) {
    if (!connected) return
    ancLevel = level
    deviceWatchProc.write("anc_level " + level + "\n")
  }

  function setAncAdaptive(enabled) {
    if (!connected) return
    ancAdaptive = enabled
    deviceWatchProc.write("anc_adaptive " + (enabled ? "on" : "off") + "\n")
  }

  function setVoicePrompt(val) {
    if (!connected) return
    voicePrompt = val
    deviceWatchProc.write("voice_prompt " + val + "\n")
  }

  function setProximity(enabled) {
    if (!connected) return
    proximity = enabled
    deviceWatchProc.write("proximity " + (enabled ? "on" : "off") + "\n")
  }

  function setLighting(effect) {
    if (!connected) return
    lighting = effect
    var r = Math.round(accent.r * 255)
    var g = Math.round(accent.g * 255)
    var b = Math.round(accent.b * 255)
    deviceWatchProc.write("lighting " + effect + " " + r + " " + g + " " + b + "\n")
  }

  function cycleListeningMode() {
    if (!connected || pendingMode !== "") return
    if (listeningMode === "off") setListeningMode("anc")
    else if (listeningMode === "anc") setListeningMode("ambient")
    else setListeningMode("off")
  }

  function updateCallContext() {
    var active = alwaysCallContext || detectedCallContext
    if (active === requestedCallContextActive) return
    requestedCallContextActive = active
    deviceWatchProc.write("call " + (active ? "on" : "off") + "\n")
  }

  function setAlwaysCallContext(val) {
    alwaysCallContext = val
    updateCallContext()
  }

  function applyCallContext(text) {
    var result = String(text || "").trim()
    if (result !== "active" && result !== "inactive") return
    if (result === "inactive") {
      inactiveCallPolls += 1
      if (inactiveCallPolls < 2) return
    } else {
      inactiveCallPolls = 0
    }
    detectedCallContext = result === "active"
    updateCallContext()
  }

  Process {
    id: deviceWatchProc
    command: [root.watchCommand]
    running: true
    stdinEnabled: true
    stdout: SplitParser {
      onRead: function(line) { root.applyDeviceState(line) }
    }
    onExited: {
      root.deviceStatus = "waiting"
      root.receiver = false
      root.connected = false
      root.requestedCallContextActive = false
      deviceWatchRestart.restart()
    }
  }

  Timer {
    id: deviceWatchRestart
    interval: 2000
    onTriggered: deviceWatchProc.running = true
  }

  Timer {
    id: modeRequestTimeout
    interval: 3000
    onTriggered: root.pendingMode = ""
  }

  Process {
    id: callContextProc
    command: ["bash", "-c", "set -o pipefail; pactl -f json list source-outputs | jq -r 'any(.[]; . as $s | (.properties // {}) as $p | ([\"application.name\", \"application.process.binary\", \"application.id\", \"application.icon_name\", \"pipewire.access.portal.app_id\", \"node.name\", \"media.name\", \"media.filename\"] | map(($p[.] // \"\") | tostring) | join(\" \")) as $id | ($s.corked != true) and (($id | test(\"easy[ _-]?effects|pw-(record|cat)|voxtype|recognition|keepalive|/dev/null\"; \"i\") | not) and (($id | test(\"(^|[^[:alnum:]_])(webrtc|chrom(e|ium)( input)?|firefox|discord|vesktop|steam(webhelper)?|telegram|zoom|brave|vivaldi|microsoft-edge)([^[:alnum:]_]|$)\"; \"i\")) or (($p[\"media.role\"] // \"\") | test(\"^(phone|communication)$\"; \"i\"))))) | if . then \"active\" else \"inactive\" end' || printf '%s\\n' unknown"]
    stdout: StdioCollector {
      waitForEnd: true
      onStreamFinished: root.applyCallContext(text)
    }
  }

  Timer {
    interval: 2000
    running: true
    repeat: true
    triggeredOnStart: true
    onTriggered: if (!callContextProc.running) callContextProc.running = true
  }

  visible: receiver || !hideWhenReceiverMissing || deviceStatus === "permission-denied" || deviceStatus === "helper-missing"
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
    tooltipText: root.connected
      ? "ROG Cetra SpeedNova\nLeft: " + root.levelText(root.leftLevel)
        + " · Right: " + root.levelText(root.rightLevel)
        + " · Case: " + root.levelText(root.caseLevel)
        + "\nMode: " + root.listeningMode.toUpperCase()
        + (root.listeningMode === "anc" ? " (" + (root.ancAdaptive ? "Adaptive" : (root.ancLevel === 1 ? "Low" : (root.ancLevel === 2 ? "Mid" : "High"))) + ")" : "")
        + " · Mic: " + (root.callContextActive ? (root.micLive ? "Live" : "Muted") : "Media")
      : "ROG Cetra SpeedNova\n" + root.statusLabel
    onPressed: function(button) {
      if (button === Qt.RightButton) {
        root.cycleListeningMode()
      } else {
        root.toggle()
      }
    }
    onWheelMoved: function(delta) {
      if (delta !== 0) root.cycleListeningMode()
    }

    Row {
      id: barContent
      anchors.centerIn: parent
      spacing: Style.space(5)

      CetraIcon {
        width: Style.bar.iconCanvas
        height: Style.bar.iconCanvas
        iconSize: Style.bar.iconCanvas
        color: root.barColor
        anchors.verticalCenter: parent.verticalCenter
      }

      Text {
        visible: root.callContextActive && root.connected
        text: root.micLive ? "󰍬" : "󰍭"
        color: root.micLive ? root.barColor : (root.bar ? root.bar.urgent : Color.urgent)
        font.family: root.fontFamily
        font.pixelSize: Style.bar.iconFont
        renderType: Text.NativeRendering
        anchors.verticalCenter: parent.verticalCenter
      }

      Text {
        visible: root.showsPercentage
        text: root.lowestLevel + "%"
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
      anchors.fill: parent
      onCloseRequested: root.close()
      onTabRequested: function(direction) { root.switchPanel(direction) }
      onTextKey: function(t) {
        if (t === "o" || t === "O" || t === "щ" || t === "Щ") {
          root.setListeningMode("off")
        } else if (t === "n" || t === "N" || t === "т" || t === "Т") {
          root.setListeningMode("anc")
        } else if (t === "a" || t === "A" || t === "ф" || t === "Ф") {
          root.setListeningMode("ambient")
        } else if (t === "1") {
          if (root.ancAdaptive) root.setAncAdaptive(false)
          root.setAncLevel(1)
        } else if (t === "2") {
          if (root.ancAdaptive) root.setAncAdaptive(false)
          root.setAncLevel(2)
        } else if (t === "3") {
          if (root.ancAdaptive) root.setAncAdaptive(false)
          root.setAncLevel(3)
        } else if (t === "m" || t === "M" || t === "ь" || t === "Ь") {
          root.setAlwaysCallContext(!root.alwaysCallContext)
        } else if (t === "p" || t === "P" || t === "з" || t === "З") {
          root.setProximity(!root.proximity)
        }
      }
      Keys.onPressed: function(event) {
        if (event.key === Qt.Key_O) {
          root.setListeningMode("off")
          event.accepted = true
        } else if (event.key === Qt.Key_N) {
          root.setListeningMode("anc")
          event.accepted = true
        } else if (event.key === Qt.Key_A) {
          root.setListeningMode("ambient")
          event.accepted = true
        } else if (event.key === Qt.Key_1) {
          if (root.ancAdaptive) root.setAncAdaptive(false)
          root.setAncLevel(1)
          event.accepted = true
        } else if (event.key === Qt.Key_2) {
          if (root.ancAdaptive) root.setAncAdaptive(false)
          root.setAncLevel(2)
          event.accepted = true
        } else if (event.key === Qt.Key_3) {
          if (root.ancAdaptive) root.setAncAdaptive(false)
          root.setAncLevel(3)
          event.accepted = true
        }
      }

      Column {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Style.space(18)

        PanelHero {
          width: parent.width
          title: "ROG Cetra SpeedNova"
          meta: root.statusLabel
          detail: root.connected && root.lowestLevel >= 0 ? root.lowestLevel + "%" : ""
          foreground: root.foreground
          fontFamily: root.fontFamily
          iconOpacity: root.receiver ? 1.0 : 0.45
          iconComponent: Component {
            CetraIcon {
              iconSize: Style.font.display
              color: root.foreground
            }
          }
        }

        Column {
          width: parent.width
          spacing: Style.space(10)

          PanelSectionHeader {
            width: parent.width
            text: "BATTERY"
            foreground: root.foreground
            fontFamily: root.fontFamily
          }

          Row {
            id: batteryRow
            width: parent.width
            spacing: Style.space(10)

            Repeater {
              model: [
                { label: "LEFT", value: root.leftLevel, icon: "󰋋" },
                { label: "RIGHT", value: root.rightLevel, icon: "󰋋" },
                { label: "CASE", value: root.caseLevel, icon: "󰂄" }
              ]

              delegate: Rectangle {
                required property var modelData
                width: (batteryRow.width - batteryRow.spacing * 2) / 3
                implicitHeight: batteryCell.implicitHeight + Style.space(22)
                radius: Style.cornerRadius
                color: Qt.rgba(root.foreground.r, root.foreground.g, root.foreground.b, 0.06)
                border.width: 1
                border.color: Qt.rgba(root.foreground.r, root.foreground.g, root.foreground.b, 0.14)
                clip: true

                Column {
                  id: batteryCell
                  anchors.centerIn: parent
                  anchors.verticalCenterOffset: -Style.space(2)
                  spacing: Style.space(3)

                  Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: Style.space(4)

                    Text {
                      text: modelData.icon
                      color: root.dim
                      font.family: root.fontFamily
                      font.pixelSize: Style.font.caption
                    }

                    Text {
                      text: modelData.label
                      color: root.dim
                      font.family: root.fontFamily
                      font.pixelSize: Style.font.caption
                      font.bold: true
                      font.letterSpacing: 1.0
                    }
                  }

                  Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.levelText(modelData.value)
                    color: modelData.value !== null && Number(modelData.value) <= 20 ? (root.bar ? root.bar.urgent : Color.urgent) : root.foreground
                    font.family: root.fontFamily
                    font.pixelSize: Style.font.title
                    font.bold: true
                  }
                }

                Rectangle {
                  anchors.left: parent.left
                  anchors.right: parent.right
                  anchors.bottom: parent.bottom
                  height: Style.space(3)
                  color: Qt.rgba(root.foreground.r, root.foreground.g, root.foreground.b, 0.12)

                  Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    visible: modelData.value !== null
                    width: parent.width * Math.max(0, Math.min(100, Number(modelData.value || 0))) / 100.0
                    color: modelData.value !== null && Number(modelData.value) <= 20
                      ? (root.bar ? root.bar.urgent : Color.urgent)
                      : Qt.rgba(root.foreground.r, root.foreground.g, root.foreground.b, 0.55)

                    Behavior on width { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                  }
                }
              }
            }
          }
        }

        Column {
          width: parent.width
          spacing: Style.space(10)
          visible: root.connected

          PanelSectionHeader {
            width: parent.width
            text: "NOISE CONTROL"
            foreground: root.foreground
            fontFamily: root.fontFamily
          }

          Row {
            id: modeRow
            width: parent.width
            spacing: Style.space(8)

            Repeater {
              model: root.modeOptions

              delegate: Button {
                required property var modelData
                width: (modeRow.width - modeRow.spacing * 2) / 3
                text: modelData.label
                iconText: modelData.icon
                foreground: root.foreground
                fontFamily: root.fontFamily
                fontSize: Style.font.bodySmall
                bordered: true
                active: root.listeningMode === modelData.value
                enabled: root.pendingMode === ""
                onClicked: root.setListeningMode(modelData.value)
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
                  { label: "Low", value: 1 },
                  { label: "Mid", value: 2 },
                  { label: "High", value: 3 }
                ]

                delegate: Button {
                  required property var modelData
                  width: (ancLevelRow.width - ancLevelRow.spacing * 2) / 3
                  text: modelData.label
                  foreground: root.foreground
                  fontFamily: root.fontFamily
                  fontSize: Style.font.caption
                  bordered: true
                  active: root.ancLevel === modelData.value && !root.ancAdaptive
                  onClicked: {
                    if (root.ancAdaptive) root.setAncAdaptive(false)
                    root.setAncLevel(modelData.value)
                  }
                }
              }
            }

            Rectangle {
              width: parent.width
              height: Style.space(36)
              radius: Style.cornerRadius
              color: Qt.rgba(root.foreground.r, root.foreground.g, root.foreground.b, adaptiveArea.containsMouse ? 0.09 : 0.05)
              border.width: 1
              border.color: Qt.rgba(root.foreground.r, root.foreground.g, root.foreground.b, 0.14)

            Row {
              anchors.fill: parent
              anchors.leftMargin: Style.space(12)
              anchors.rightMargin: Style.space(10)
              spacing: Style.space(8)

              Text {
                text: "󰚥  Smart Adaptive ANC"
                color: root.foreground
                font.family: root.fontFamily
                font.pixelSize: Style.font.caption
                font.bold: true
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - adaptiveSwitch.width - parent.spacing
                elide: Text.ElideRight
              }

                ToggleSwitch {
                  id: adaptiveSwitch
                  anchors.verticalCenter: parent.verticalCenter
                  checked: root.ancAdaptive
                  onToggled: root.setAncAdaptive(!root.ancAdaptive)
                }
              }

              MouseArea {
                id: adaptiveArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.setAncAdaptive(!root.ancAdaptive)
              }
            }
          }

          Text {
            width: parent.width
            text: root.pendingMode !== ""
              ? "Switching to " + root.pendingMode.toUpperCase() + "…"
              : "Shortcuts: O Off · N ANC · A Ambient · 1/2/3 Level · M Mute"
            color: root.dim
            font.family: root.fontFamily
            font.pixelSize: Style.font.caption
          }
        }

        Column {
          width: parent.width
          spacing: Style.space(10)
          visible: root.connected

          PanelSectionHeader {
            width: parent.width
            text: "MICROPHONE"
            foreground: root.foreground
            fontFamily: root.fontFamily
          }

          Rectangle {
            width: parent.width
            implicitHeight: microphoneState.implicitHeight + Style.space(18)
            radius: Style.cornerRadius
            color: Qt.rgba(root.foreground.r, root.foreground.g, root.foreground.b, 0.06)
            border.width: 1
            border.color: root.callContextActive && !root.micLive
              ? (root.bar ? root.bar.urgent : Color.urgent)
              : Qt.rgba(root.foreground.r, root.foreground.g, root.foreground.b, 0.14)

            Row {
              id: microphoneState
              anchors.left: parent.left
              anchors.right: parent.right
              anchors.margins: Style.space(12)
              anchors.verticalCenter: parent.verticalCenter
              spacing: Style.space(12)

              Text {
                text: root.callContextActive ? (root.micLive ? "󰍬" : "󰍭") : "󰋋"
                color: root.callContextActive
                  ? (root.micLive ? root.foreground : (root.bar ? root.bar.urgent : Color.urgent))
                  : root.dim
                font.family: root.fontFamily
                font.pixelSize: Style.font.display
                anchors.verticalCenter: parent.verticalCenter
              }

              Column {
                width: parent.width - parent.children[0].width - parent.spacing
                spacing: Style.space(3)

                Text {
                  text: root.callContextActive
                    ? (root.micLive ? "Microphone Live" : "Microphone Muted")
                    : "Media Gesture Active"
                  color: root.callContextActive && !root.micLive
                    ? (root.bar ? root.bar.urgent : Color.urgent)
                    : root.foreground
                  font.family: root.fontFamily
                  font.pixelSize: Style.font.bodySmall
                  font.bold: true
                }

                Text {
                  text: root.callContextActive
                    ? (root.micLive
                        ? "Tap right earbud to mute · Spoken: microphone off"
                        : "Tap right earbud to unmute · Spoken: microphone on")
                    : "Right earbud controls media playback outside calls"
                  color: root.dim
                  font.family: root.fontFamily
                  font.pixelSize: Style.font.caption
                }
              }
            }
          }
        }

        Column {
          width: parent.width
          spacing: Style.space(8)
          visible: root.connected

          PanelSectionHeader {
            width: parent.width
            text: "LIGHTING"
            foreground: root.foreground
            fontFamily: root.fontFamily
          }

          Row {
            id: lightingRow1
            width: parent.width
            spacing: Style.space(6)

            Button {
              width: (lightingRow1.width - lightingRow1.spacing) / 2
              text: "Off"
              iconText: "󰌶"
              foreground: root.foreground
              fontFamily: root.fontFamily
              fontSize: Style.font.bodySmall
              bordered: true
              active: root.lighting === "off"
              onClicked: root.setLighting("off")
            }

            Button {
              width: (lightingRow1.width - lightingRow1.spacing) / 2
              text: "Color Cycle"
              iconText: "󰏘"
              foreground: root.foreground
              fontFamily: root.fontFamily
              fontSize: Style.font.bodySmall
              bordered: true
              active: root.lighting === "cycle"
              onClicked: root.setLighting("cycle")
            }
          }

          Row {
            id: lightingRow2
            width: parent.width
            spacing: Style.space(6)

            Repeater {
              model: [
                { label: "Static", value: "static", icon: "󰌵" },
                { label: "Breathing", value: "breathing", icon: "󰈈" },
                { label: "Strobing", value: "strobing", icon: "󱐋" }
              ]

              delegate: Button {
                required property var modelData
                width: (lightingRow2.width - lightingRow2.spacing * 2) / 3
                text: modelData.label
                iconText: modelData.icon
                foreground: root.foreground
                fontFamily: root.fontFamily
                fontSize: Style.font.caption
                bordered: true
                active: root.lighting === modelData.value
                onClicked: root.setLighting(modelData.value)
              }
            }
          }
        }

        Column {
          width: parent.width
          spacing: Style.space(8)
          visible: root.connected

          PanelSectionHeader {
            width: parent.width
            text: "DEVICE SETTINGS"
            foreground: root.foreground
            fontFamily: root.fontFamily
          }

          Row {
            id: voicePromptRow
            width: parent.width
            spacing: Style.space(6)

            Repeater {
              model: [
                { label: "English", value: "english", icon: "󰗊" },
                { label: "Chinese", value: "chinese", icon: "󰗊" },
                { label: "Beeps", value: "sound", icon: "󰓎" }
              ]

              delegate: Button {
                required property var modelData
                width: (voicePromptRow.width - voicePromptRow.spacing * 2) / 3
                text: modelData.label
                iconText: modelData.icon
                foreground: root.foreground
                fontFamily: root.fontFamily
                fontSize: Style.font.caption
                bordered: true
                active: root.voicePrompt === modelData.value
                onClicked: root.setVoicePrompt(modelData.value)
              }
            }
          }

          Rectangle {
            width: parent.width
            height: Style.space(36)
            radius: Style.cornerRadius
            color: Qt.rgba(root.foreground.r, root.foreground.g, root.foreground.b, proximityArea.containsMouse ? 0.09 : 0.05)
            border.width: 1
            border.color: Qt.rgba(root.foreground.r, root.foreground.g, root.foreground.b, 0.14)

            Row {
              anchors.fill: parent
              anchors.leftMargin: Style.space(12)
              anchors.rightMargin: Style.space(10)
              spacing: Style.space(8)

              Text {
                text: "󰏤  In-Ear Auto-Pause"
                color: root.foreground
                font.family: root.fontFamily
                font.pixelSize: Style.font.caption
                font.bold: true
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - proximitySwitch.width - parent.spacing
                elide: Text.ElideRight
              }

              ToggleSwitch {
                id: proximitySwitch
                anchors.verticalCenter: parent.verticalCenter
                checked: root.proximity
                onToggled: root.setProximity(!root.proximity)
              }
            }

            MouseArea {
              id: proximityArea
              anchors.fill: parent
              hoverEnabled: true
              cursorShape: Qt.PointingHandCursor
              onClicked: root.setProximity(!root.proximity)
            }
          }

          Rectangle {
            width: parent.width
            height: Style.space(36)
            radius: Style.cornerRadius
            color: Qt.rgba(root.foreground.r, root.foreground.g, root.foreground.b, alwaysMuteArea.containsMouse ? 0.09 : 0.05)
            border.width: 1
            border.color: Qt.rgba(root.foreground.r, root.foreground.g, root.foreground.b, 0.14)

            Row {
              anchors.fill: parent
              anchors.leftMargin: Style.space(12)
              anchors.rightMargin: Style.space(10)
              spacing: Style.space(8)

              Text {
                text: "󰍭  Always Mute Gesture"
                color: root.foreground
                font.family: root.fontFamily
                font.pixelSize: Style.font.caption
                font.bold: true
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - alwaysMuteSwitch.width - parent.spacing
                elide: Text.ElideRight
              }

              ToggleSwitch {
                id: alwaysMuteSwitch
                anchors.verticalCenter: parent.verticalCenter
                checked: root.alwaysCallContext
                onToggled: root.setAlwaysCallContext(!root.alwaysCallContext)
              }
            }

            MouseArea {
              id: alwaysMuteArea
              anchors.fill: parent
              hoverEnabled: true
              cursorShape: Qt.PointingHandCursor
              onClicked: root.setAlwaysCallContext(!root.alwaysCallContext)
            }
          }
        }
      }
    }
  }
}
