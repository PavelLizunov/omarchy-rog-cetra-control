import QtQuick
import Quickshell
import Quickshell.Io
import qs.Commons
import qs.Ui

Panel {
  id: root
  moduleName: "io.github.pavellizunov.rog-cetra-battery"
  manageIpc: false

  readonly property string statusCommand: Qt.resolvedUrl("bin/cetra-status").toString().replace("file://", "")
  readonly property color foreground: bar ? bar.foreground : Color.foreground
  readonly property color barColor: lowestLevel >= 0 && lowestLevel <= 20
    ? (bar ? bar.urgent : Color.urgent)
    : barForeground
  readonly property color dim: Qt.rgba(foreground.r, foreground.g, foreground.b, 0.58)
  readonly property string fontFamily: bar ? bar.fontFamily : Style.font.family

  readonly property bool showPercentage: setting("showPercentage", true) === true
  readonly property bool hideWhenReceiverMissing: setting("hideWhenReceiverMissing", true) === true
  readonly property int refreshIntervalMs: Math.max(2, Number(setting("refreshIntervalSec", 5))) * 1000
  readonly property int openRefreshIntervalMs: Math.max(1, Number(setting("openRefreshIntervalSec", 2))) * 1000

  property string deviceStatus: "starting"
  property bool receiver: false
  property bool connected: false
  property var leftLevel: null
  property var rightLevel: null
  property var caseLevel: null
  property int failedSamples: 0
  property bool refreshPending: false
  property bool hasFreshData: false

  readonly property int lowestLevel: {
    var levels = []
    if (leftLevel !== null) levels.push(Number(leftLevel))
    if (rightLevel !== null) levels.push(Number(rightLevel))
    return levels.length ? Math.min.apply(null, levels) : -1
  }

  readonly property bool showsPercentage: showPercentage && connected && lowestLevel >= 0 && !button.vertical
  readonly property string statusLabel: {
    if (deviceStatus === "helper-missing") return "Battery helper is not installed"
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

  function recordFailedSample(status) {
    deviceStatus = status || "error"
    failedSamples++
    if (failedSamples < 3 && hasFreshData) return
    if (status === "receiver-missing" || status === "helper-missing" || status === "permission-denied") {
      receiver = false
      connected = false
      leftLevel = null
      rightLevel = null
      caseLevel = null
    }
  }

  function applyStatus(text) {
    var data
    try {
      data = JSON.parse(String(text || "").trim())
    } catch (error) {
      recordFailedSample("invalid-output")
      return
    }

    var status = String(data.status || "unknown")
    receiver = data.receiver === true
    if (status !== "ok") {
      recordFailedSample(status)
      return
    }

    failedSamples = 0
    hasFreshData = true
    deviceStatus = "ok"
    receiver = data.receiver === true
    connected = data.connected === true
    leftLevel = data.left === undefined ? null : data.left
    rightLevel = data.right === undefined ? null : data.right
    caseLevel = data.case === undefined ? null : data.case
  }

  function refresh() {
    if (statusProc.running) {
      refreshPending = true
      return
    }
    refreshPending = false
    statusProc.running = true
    processWatchdog.restart()
  }

  function finishRefresh() {
    processWatchdog.stop()
    if (refreshPending) Qt.callLater(root.refresh)
  }

  Component.onCompleted: refresh()
  onOpenedChanged: if (opened) refreshDebounce.restart()

  Process {
    id: statusProc
    command: [root.statusCommand]
    stdout: StdioCollector {
      waitForEnd: true
      onStreamFinished: root.applyStatus(text)
    }
    onExited: root.finishRefresh()
  }

  Timer {
    id: refreshDebounce
    interval: 100
    onTriggered: root.refresh()
  }

  Timer {
    id: processWatchdog
    interval: 3500
    onTriggered: {
      if (statusProc.running) statusProc.running = false
      root.recordFailedSample("timeout")
      root.finishRefresh()
    }
  }

  Timer {
    interval: root.opened ? root.openRefreshIntervalMs : root.refreshIntervalMs
    running: true
    repeat: true
    onTriggered: root.refresh()
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
        + "\nRight: " + root.levelText(root.rightLevel)
        + "\nCase: " + root.levelText(root.caseLevel)
      : "ROG Cetra SpeedNova\n" + root.statusLabel
    onPressed: function() { root.toggle() }

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
    contentWidth: panel.fittedContentWidth(Style.space(360))
    contentHeight: panel.fittedContentHeight(column.implicitHeight)

    PanelKeyCatcher {
      id: keyCatcher
      anchors.fill: parent
      onCloseRequested: root.close()
      onTabRequested: function(direction) { root.switchPanel(direction) }

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
                { label: "LEFT", value: root.leftLevel },
                { label: "RIGHT", value: root.rightLevel },
                { label: "CASE", value: root.caseLevel }
              ]

              delegate: Rectangle {
                required property var modelData
                width: (batteryRow.width - batteryRow.spacing * 2) / 3
                implicitHeight: batteryCell.implicitHeight + Style.space(18)
                radius: Style.cornerRadius
                color: Qt.rgba(root.foreground.r, root.foreground.g, root.foreground.b, 0.06)
                border.width: 1
                border.color: Qt.rgba(root.foreground.r, root.foreground.g, root.foreground.b, 0.14)

                Column {
                  id: batteryCell
                  anchors.centerIn: parent
                  spacing: Style.space(3)

                  Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: modelData.label
                    color: root.dim
                    font.family: root.fontFamily
                    font.pixelSize: Style.font.caption
                    font.bold: true
                    font.letterSpacing: 1.0
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
              }
            }
          }
        }

        PanelSeparator {
          foreground: root.foreground
        }

        Text {
          width: parent.width
          text: "Battery data is read directly from the ASUS USB receiver. The plugin does not change audio, microphone, firmware, or headset settings."
          wrapMode: Text.WordWrap
          color: root.dim
          font.family: root.fontFamily
          font.pixelSize: Style.font.caption
        }
      }
    }
  }
}
