import QtQuick
import Quickshell
import Quickshell.Io
import qs.Commons

CetraPreferences {
  id: root
  readonly property var serviceHost: root

  readonly property string watchCommand: Qt.resolvedUrl("bin/cetra-watch").toString().replace("file://", "")
  property alias callContextProc: detector.process
  property alias callContextTimeout: detector.timeout
  CallDetector { id: detector; root: serviceHost }
  readonly property color themeColor: Color.accent
  property string sessionLightingEffect: ""
  property string lastLightingPayload: ""
  readonly property bool autoThemeColor: settings.autoThemeColor === true
  onThemeColorChanged: scheduleThemeColor()
  onAutoThemeColorChanged: { if (!autoThemeColor) themeColorDelay.stop() }

  function scheduleThemeColor() {
    if (autoThemeColor && settings.useThemeColor !== false && sessionLightingEffect !== "" && connected)
      themeColorDelay.restart()
  }

  function applyThemeColor() {
    if (!autoThemeColor || settings.useThemeColor === false || sessionLightingEffect === ""
        || lighting !== sessionLightingEffect || !connected)
      return false
    return setLighting(sessionLightingEffect, themeColor, true)
  }

  function setAutoThemeColor(enabled) {
    if (typeof enabled !== "boolean")
      return false
    var changed = updateSetting("autoThemeColor", enabled, {})
    if (changed && enabled && settings.useThemeColor !== false
        && ["static", "breathing", "strobing"].indexOf(lighting) >= 0)
      setLighting(lighting, themeColor)
    return changed
  }

  Timer {
    id: themeColorDelay
    interval: 350
    onTriggered: root.applyThemeColor()
  }

  property string deviceStatus: "starting"
  property bool receiver: false
  property bool connected: false
  property var leftPresent: null
  property var rightPresent: null
  property var leftCharging: null
  property var rightCharging: null
  property var caseCharging: null
  property bool presenceObserved: false
  property var leftLevel: null
  property var rightLevel: null
  property var caseLevel: null
  property string listeningMode: "unknown"
  property string pendingMode: ""
  property bool modeRequestTimedOut: false
  property var ancLevel: null
  property var ancAdaptive: null
  property string voicePrompt: "unknown"
  property var proximity: null
  property var pendingSettings: ({})
  property var timedOutSettings: ({})
  property bool settingsRequestTimedOut: false
  readonly property string settingsStatusKey: settingsRequestTimedOut ? "settings.notConfirmed" : Object.keys(pendingSettings).length ? "settings.pending" : ""
  property string lighting: "unknown"
  property bool callContextActive: false
  property bool requestedCallContextActive: false
  property bool detectedCallContext: false
  property int inactiveCallPolls: 0
  property int unknownCallPolls: 0
  property int nonpositiveCallPolls: 0

  onReceiverChanged: {
    if (!receiver) {
      resetCallDetection()
      updateCallContext()
    }
  }

  function resetCallDetection() {
    detectedCallContext = false
    inactiveCallPolls = unknownCallPolls = nonpositiveCallPolls = 0
    callContextProc.pending = false
    callContextTimeout.stop()
  }

  function clearDeviceState() {
    sessionLightingEffect = ""
    lastLightingPayload = ""
    themeColorDelay.stop()
    connected = false
    leftLevel = rightLevel = caseLevel = null
    leftPresent = rightPresent = null
    leftCharging = rightCharging = caseCharging = null
    presenceObserved = false
    listeningMode = "unknown"
    pendingMode = ""
    modeRequestTimedOut = false
    ancLevel = ancAdaptive = proximity = null
    voicePrompt = "unknown"
    lighting = "unknown"
    pendingSettings = ({})
    timedOutSettings = ({})
    settingsRequestTimedOut = false
    settingsRequestTimeout.stop()
    callContextActive = false
    modeRequestTimeout.stop()
  }

  function applyStatus(text) {
    var data
    try {
      data = JSON.parse(String(text || "").trim())
    } catch (error) {
      return
    }
    if (!data || typeof data !== "object" || Array.isArray(data))
      return

    var status = String(data.status || "unknown")
    receiver = data.receiver === true
    if (status !== "ok") {
      deviceStatus = status
      clearDeviceState()
      return
    }

    deviceStatus = "ok"
    receiver = data.receiver === true
    if (!receiver) {
      clearDeviceState()
      return
    }
    leftPresent = typeof data.left_present === "boolean" ? data.left_present : null
    rightPresent = typeof data.right_present === "boolean" ? data.right_present : null
    leftCharging = typeof data.left_charging === "boolean" ? data.left_charging : null
    rightCharging = typeof data.right_charging === "boolean" ? data.right_charging : null
    caseCharging = typeof data.case_charging === "boolean" ? data.case_charging : null
    presenceObserved = data.presence_raw !== undefined && data.presence_raw !== null
    // Once presence has been observed, stale battery percentages cannot restore controls.
    connected = receiver && (leftPresent === true || rightPresent === true
      || (!presenceObserved && leftPresent === null && rightPresent === null && data.connected === true))
    if (!connected) {
      pendingMode = ""
      modeRequestTimeout.stop()
      pendingSettings = ({})
      timedOutSettings = ({})
      settingsRequestTimedOut = false
      settingsRequestTimeout.stop()
    }
    leftLevel = data.left === undefined ? null : data.left
    rightLevel = data.right === undefined ? null : data.right
    caseLevel = data.case === undefined ? null : data.case
    listeningMode = String(data.mode || "unknown")
    ancLevel = connected && [1, 2, 3].indexOf(data.anc_level) >= 0 ? data.anc_level : null
    ancAdaptive = connected && typeof data.anc_adaptive === "boolean" ? data.anc_adaptive : null
    voicePrompt = connected && ["english", "chinese", "sound"].indexOf(data.voice_prompt) >= 0 ? data.voice_prompt : "unknown"
    proximity = connected && typeof data.proximity === "boolean" ? data.proximity : null
    var confirmed = { anc_level: ancLevel, anc_adaptive: ancAdaptive, voice_prompt: voicePrompt, proximity: proximity }
    var unconfirmed = {}
    for (var failed in timedOutSettings)
      if (confirmed[failed] !== timedOutSettings[failed])
        unconfirmed[failed] = timedOutSettings[failed]
    timedOutSettings = unconfirmed
    settingsRequestTimedOut = Object.keys(unconfirmed).length > 0
    var remaining = {}
    for (var command in pendingSettings)
      if (confirmed[command] !== pendingSettings[command].desired)
        remaining[command] = pendingSettings[command]
    pendingSettings = remaining
    if (!Object.keys(remaining).length)
      settingsRequestTimeout.stop()
    if (data.lighting !== undefined)
      lighting = String(data.lighting)
    // Gesture counts cannot establish the headset's absolute mute state.
    callContextActive = data.call_context === true
    if (pendingMode !== "" && listeningMode === pendingMode) {
      pendingMode = ""
      modeRequestTimedOut = false
      modeRequestTimeout.stop()
    }
  }

  function applyDeviceState(text) {
    try {
      var data = JSON.parse(String(text || "").trim())
      if (data && typeof data === "object" && typeof data.status === "string")
        applyStatus(text)
    } catch (error) {}
  }

  function setListeningMode(mode) {
    if (!connected || !deviceWatchProc.running || pendingMode !== ""
        || ["off", "anc", "ambient"].indexOf(mode) < 0)
      return
    modeRequestTimedOut = false
    pendingMode = mode
    modeRequestTimeout.restart()
    deviceWatchProc.write("mode " + mode + "\n")
  }

  function setAncLevel(level) {
    if ([1, 2, 3].indexOf(level) < 0)
      return false
    return requestSetting("anc_level", level, String(level))
  }

  function setAncAdaptive(enabled) {
    if (typeof enabled !== "boolean")
      return false
    return requestSetting("anc_adaptive", enabled, enabled ? "on" : "off")
  }

  function setVoicePrompt(val) {
    if (["english", "chinese", "sound"].indexOf(val) < 0)
      return false
    return requestSetting("voice_prompt", val, val)
  }

  function setProximity(enabled) {
    if (typeof enabled !== "boolean")
      return false
    return requestSetting("proximity", enabled, enabled ? "on" : "off")
  }

  function requestSetting(command, desired, argument) {
    if (!connected || !deviceWatchProc.running || pendingSettings[command] !== undefined)
      return false
    var requests = Object.assign({}, pendingSettings)
    // Allow one nominal 10-second readback cycle plus margin. Tick-based,
    // not a wall-clock deadline; immediate readback can still contain old data.
    requests[command] = { desired: desired, remainingTicks: 48 }
    pendingSettings = requests
    settingsRequestTimedOut = false
    timedOutSettings = ({})
    if (!settingsRequestTimeout.running)
      settingsRequestTimeout.start()
    deviceWatchProc.write(command + " " + argument + "\n")
    return true
  }

  function expireSettingsRequests() {
    var remaining = {}
    var failed = Object.assign({}, timedOutSettings)
    for (var command in pendingSettings) {
      var request = pendingSettings[command]
      if (request.remainingTicks <= 1) {
        settingsRequestTimedOut = true
        failed[command] = request.desired
      } else
        remaining[command] = { desired: request.desired, remainingTicks: request.remainingTicks - 1 }
    }
    pendingSettings = remaining
    timedOutSettings = failed
    if (!Object.keys(remaining).length)
      settingsRequestTimeout.stop()
  }

  function setLighting(effect, color, automatic) {
    if (!connected || !deviceWatchProc.running
        || ["off", "cycle", "static", "breathing", "strobing"].indexOf(effect) < 0)
      return false
    var r = 0, g = 0, b = 0
    if (effect !== "off" && effect !== "cycle") {
      if (!color)
        return false
      var channels = [color.r, color.g, color.b]
      for (var i = 0; i < channels.length; i++)
        if (typeof channels[i] !== "number" || !isFinite(channels[i]) || channels[i] < 0 || channels[i] > 1)
          return false
      r = Math.round(color.r * 255)
      g = Math.round(color.g * 255)
      b = Math.round(color.b * 255)
    }
    var payload = "lighting " + effect + " " + r + " " + g + " " + b + "\n"
    if (automatic === true && payload === lastLightingPayload)
      return false
    // Authorization is session-local; daemon JSON owns last-sent status.
    // Dispatch does not establish physical color readback.
    deviceWatchProc.write(payload)
    sessionLightingEffect = ["static", "breathing", "strobing"].indexOf(effect) >= 0 ? effect : ""
    lastLightingPayload = payload
    if (sessionLightingEffect === "") themeColorDelay.stop()
    return true
  }

  function updateCallContext(force) {
    var active = detectedCallContext
    if (!hostReady || !deviceWatchProc.running || (!force && active === requestedCallContextActive))
      return
    deviceWatchProc.write("call " + (active ? "on" : "off") + "\n")
    // Sent intent, not an acknowledgement from the receiver.
    requestedCallContextActive = active
  }

  function applyCallContext(text) {
    var result = String(text || "").trim()
    // Alternating inactive/unknown must not keep automatic call mode alive.
    nonpositiveCallPolls = result === "active" ? 0 : Math.min(nonpositiveCallPolls + 1, 3)
    if (result !== "active" && result !== "inactive") {
      inactiveCallPolls = 0
      unknownCallPolls = Math.min(unknownCallPolls + 1, 3)
      if (nonpositiveCallPolls >= 3) {
        detectedCallContext = false
        updateCallContext()
      }
      return
    }
    unknownCallPolls = 0
    if (result === "inactive") {
      inactiveCallPolls = Math.min(inactiveCallPolls + 1, 2)
      if (inactiveCallPolls < 2 && nonpositiveCallPolls < 3)
        return
    } else {
      inactiveCallPolls = 0
    }
    detectedCallContext = result === "active"
    updateCallContext()
  }

  function finishCallContext(text) {
    // Exit, launch failure and watchdog can race; consume each poll once.
    if (!callContextProc.pending)
      return
    callContextProc.pending = false
    callContextTimeout.stop()
    applyCallContext(text)
  }

  function watcherStopped() {
    // FailedToStart has no exited signal; normal exit can notify both handlers.
    if (deviceWatchProc.stopHandled)
      return
    deviceWatchProc.stopHandled = true
    deviceStatus = "helper-error"
    receiver = false
    clearDeviceState()
    requestedCallContextActive = false
    if (hostReady)
      deviceWatchRestart.restart()
  }

  Process {
    id: deviceWatchProc
    property bool stopHandled: false
    command: [root.watchCommand]
    running: root.hostReady && !deviceWatchRestart.running
    stdinEnabled: true
    onStarted: {
      deviceWatchRestart.stop()
      root.updateCallContext(true)
    }
    onRunningChanged: {
      if (deviceWatchProc.running)
        deviceWatchProc.stopHandled = false
      else
        root.watcherStopped()
    }
    stdout: SplitParser {
      onRead: function (line) {
        root.applyDeviceState(line)
      }
    }
    onExited: root.watcherStopped()
  }

  Timer {
    id: deviceWatchRestart
    interval: 2000
    onTriggered: deviceWatchProc.stopHandled = false
  }

  Timer {
    id: settingsRequestTimeout
    interval: 250
    repeat: true
    onTriggered: root.expireSettingsRequests()
  }

  Timer {
    id: modeRequestTimeout
    interval: 3000
    onTriggered: {
      if (root.pendingMode !== "") {
        root.modeRequestTimedOut = true
        root.pendingMode = ""
      }
    }
  }

}
