import QtQuick
import qs.Commons
import qs.Commons as Host
import qs.Ui

// View-facing projection and actions. No processes or device ownership.
Panel {
  id: root
  moduleName: "io.github.pavellizunov.rog-cetra-control"
  manageIpc: false
  property var i18n
  readonly property var service: bar?.shell?.serviceFor(root.moduleName) || null
  function tr(key, fallback, params) {
    return i18n.text(key, fallback, params)
  }
  function preference(name, fallback) {
    var current = service ? service.settings : settings
    return current && current[name] !== undefined && current[name] !== null ? current[name] : fallback
  }
  readonly property color foreground: bar ? bar.foreground : Host.Color.foreground
  readonly property color accent: accentFor(bar)
  function accentFor(host) {
    return host && host.accent !== undefined ? host.accent : Host.Color.accent
  }
  readonly property color warningColor: readableWarning(bar ? bar.urgent : Host.Color.urgent, Host.Color.popups.background, foreground)
  readonly property color barColor: lowestLevel >= 0 && lowestLevel <= 20
    ? readableWarning(bar ? bar.urgent : Host.Color.urgent, Host.Color.bar.background, barForeground) : barForeground
  function luminance(color) {
    var channels = [color.r, color.g, color.b].map(function (v) {
      return v <= 0.04045 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4)
    })
    return channels[0] * 0.2126 + channels[1] * 0.7152 + channels[2] * 0.0722
  }
  function readableWarning(candidate, background, fallback) {
    var a = luminance(candidate), b = luminance(background)
    return (Math.max(a, b) + 0.05) / (Math.min(a, b) + 0.05) >= 4.5 ? candidate : fallback
  }
  readonly property color dim: Qt.rgba(foreground.r, foreground.g, foreground.b, 0.85)
  readonly property color rule: Qt.rgba(foreground.r, foreground.g, foreground.b, 0.12)
  readonly property string fontFamily: bar ? bar.fontFamily : Style.font.family
  property string panelPage: "sound"
  property string devicePage: "settings"
  property var lightingDraftRgb: [255, 255, 255]
  property bool lightingDraftError: false
  readonly property bool settingsExpanded: panelPage === "device"
  property bool lightingColorExpanded: false
  property bool languageExpanded: false
  property string lightingFeedback: ""
  readonly property bool showMicLevel: preference("showMicLevel", false) === true
  readonly property string microphoneCaptureState: service && service.microphoneCaptureState !== undefined ? service.microphoneCaptureState : "unknown"
  readonly property var microphoneLevel: service && service.microphoneLevel !== undefined ? service.microphoneLevel : null
  readonly property bool hideWhenReceiverMissing: preference("hideWhenReceiverMissing", true) === true
  readonly property bool hideWhenDisconnected: preference("hideWhenDisconnected", hideWhenReceiverMissing) === true
  readonly property bool panelAvailable: receiver || bluetoothAudioConnected === true
  readonly property var bluetoothAudioConnected: service ? service.bluetoothAudioConnected : null
  readonly property string bluetoothAvailability: service ? service.bluetoothAvailability : "unavailable"
  readonly property var bluetoothCandidates: service ? service.bluetoothCandidates : []
  readonly property string audioIdentity: service ? service.audioIdentity : "missing"
  readonly property var bluetoothBattery: service ? service.bluetoothBattery : null
  readonly property var audioStatus: service ? service.audioStatus : ({ output: "unknown", capture: "unknown", profile: "unknown", sink: false, microphone: false })
  property bool bluetoothExpanded: false
  property var connectionDetailsButton: null
  function selectBluetoothAudio(address) {
    if (service && service.selectBluetoothAudio(address)) bluetoothExpanded = false
  }
  function connectionText(value) {
    return value === true ? root.tr("connection.connected", "Connected") : value === false
      ? root.tr("connection.disconnected", "Disconnected") : root.tr("connection.unknown", "Unknown")
  }
  function routeText(value) {
    var labels = { usb: "USB", bluetooth: "Bluetooth", other: root.tr("connection.other", "Other device"),
      mixed: root.tr("connection.mixed", "Multiple devices"), inactive: root.tr("connection.inactive", "Inactive") }
    return labels[value] || root.tr("connection.unknown", "Unknown")
  }
  function bluetoothProfileText() {
    if (audioStatus.profile === "a2dp") return root.tr("connection.a2dp", "A2DP: playback; microphone unavailable in this profile.")
    if (audioStatus.profile === "hfp" && audioStatus.microphone) return root.tr("connection.hfp", "Headset profile: microphone endpoint available.")
    if (audioStatus.profile === "hfp") return root.tr("connection.hfpNoMic", "Headset profile: microphone endpoint not observed.")
    return root.tr("connection.endpoints", "Profile unknown. Playback endpoint: {sink}; microphone endpoint: {source}.", {
      sink: endpointText(audioStatus.sink), source: endpointText(audioStatus.microphone) })
  }
  function endpointText(value) {
    return value ? root.tr("connection.endpointPresent", "Available") : root.tr("connection.endpointAbsent", "Not observed")
  }
  function bluetoothDeviceText(candidate) {
    return String(candidate.name).slice(0, 100) + "\n" + candidate.address
  }
  function bluetoothBatteryDetail() {
    if (!bluetoothBattery || bluetoothBattery.percent === null) return bluetoothBatteryText()
    return bluetoothBatteryText() + "\n" + (bluetoothBattery.snapshot === "property-update"
      ? root.tr("bluetooth.updated", "System property changed during this connection.")
      : root.tr("bluetooth.cached", "Cached system value; no new hardware report confirmed."))
  }
  function bluetoothBatteryText() {
    if (!bluetoothBattery || bluetoothBattery.percent === null) return root.tr("bluetooth.noBattery", "Bluetooth reported charge: no data.")
    return root.tr("bluetooth.battery", "Bluetooth reported charge: {value}%. Side and freshness unknown.", { value: bluetoothBattery.percent })
  }

  readonly property string deviceStatus: service ? service.deviceStatus : "starting"
  readonly property bool receiver: service ? service.receiver : false
  readonly property bool connected: service ? service.connected : false
  readonly property var bluetoothTelemetry: service && service.bluetoothTelemetry !== undefined ? service.bluetoothTelemetry : null
  readonly property string bluetoothTelemetryState: service ? service.bluetoothTelemetryState : "idle"
  readonly property bool bluetoothTelemetryBusy: service ? service.bluetoothTelemetryBusy : false
  readonly property bool bluetoothTelemetryCanRefresh: service ? service.bluetoothTelemetryCanRefresh : false
  readonly property bool usesBluetoothTelemetry: !connected && bluetoothTelemetry !== null
  readonly property string bluetoothTelemetryMode: bluetoothTelemetry && bluetoothTelemetry.mode !== undefined ? bluetoothTelemetry.mode : "unknown"
  readonly property string batterySource: connected ? "usb" : (usesBluetoothTelemetry ? "bluetooth" : "unknown")

  readonly property var leftPresent: usesBluetoothTelemetry ? null : (service ? service.leftPresent : null)
  readonly property var rightPresent: usesBluetoothTelemetry ? null : (service ? service.rightPresent : null)
  readonly property var leftCharging: connected ? (service ? service.leftCharging : null) : (usesBluetoothTelemetry && bluetoothTelemetry.left_charging !== undefined ? bluetoothTelemetry.left_charging : null)
  readonly property var rightCharging: connected ? (service ? service.rightCharging : null) : (usesBluetoothTelemetry && bluetoothTelemetry.right_charging !== undefined ? bluetoothTelemetry.right_charging : null)
  readonly property var caseCharging: connected ? (service ? service.caseCharging : null) : (usesBluetoothTelemetry && bluetoothTelemetry.case_charging !== undefined ? bluetoothTelemetry.case_charging : null)
  readonly property bool presenceObserved: usesBluetoothTelemetry ? false : (service ? service.presenceObserved : false)
  readonly property var leftLevel: connected ? (service ? service.leftLevel : null) : (usesBluetoothTelemetry && bluetoothTelemetry.left !== undefined ? bluetoothTelemetry.left : null)
  readonly property var rightLevel: connected ? (service ? service.rightLevel : null) : (usesBluetoothTelemetry && bluetoothTelemetry.right !== undefined ? bluetoothTelemetry.right : null)
  readonly property var caseLevel: connected ? (service ? service.caseLevel : null) : (usesBluetoothTelemetry && bluetoothTelemetry.case !== undefined ? bluetoothTelemetry.case : null)
  readonly property string listeningMode: service ? service.listeningMode : "unknown"
  readonly property string pendingMode: service ? service.pendingMode : ""
  readonly property bool modeRequestTimedOut: service ? service.modeRequestTimedOut : false
  readonly property var ancLevel: service ? service.ancLevel : null
  readonly property var ancAdaptive: service ? service.ancAdaptive : null
  readonly property string voicePrompt: service ? service.voicePrompt : "unknown"
  readonly property var pendingSettings: service ? service.pendingSettings : ({})
  readonly property bool settingsRequestTimedOut: service ? service.settingsRequestTimedOut : false
  readonly property string settingsStatusKey: service ? service.settingsStatusKey : ""
  readonly property string lighting: service ? service.lighting : "unknown"
  readonly property bool useThemeColor: preference("useThemeColor", true) === true
  readonly property bool autoThemeColor: preference("autoThemeColor", false) === true
  readonly property var lightingRgb: {
    var current = (service ? service.settings : settings) || {}
    return ["lightingRed", "lightingGreen", "lightingBlue"].map(function (key) {
      return current[key] === undefined ? 255 : current[key]
    })
  }
  readonly property var selectedLightingColor: {
    if (useThemeColor)
      return root.accent
    for (var i = 0; i < lightingRgb.length; i++)
      if (typeof lightingRgb[i] !== "number" || !isFinite(lightingRgb[i])
          || Math.floor(lightingRgb[i]) !== lightingRgb[i] || lightingRgb[i] < 0 || lightingRgb[i] > 255)
        return null
    return { r: lightingRgb[0] / 255, g: lightingRgb[1] / 255, b: lightingRgb[2] / 255 }
  }
  readonly property string colorApplyEffect: ["static", "breathing", "strobing"].indexOf(lighting) >= 0 ? lighting : "static"
  readonly property bool callContextActive: service ? service.callContextActive : false
  readonly property int lowestLevel: {
    var levels = []
    if (leftLevel !== null && leftLevel !== undefined && leftLevel >= 0 && leftLevel <= 100) levels.push(Number(leftLevel))
    if (rightLevel !== null && rightLevel !== undefined && rightLevel >= 0 && rightLevel <= 100) levels.push(Number(rightLevel))
    return levels.length ? Math.min.apply(null, levels) : -1
  }

  property var _registeredService: null
  function _updateBluetoothPanelOpen() {
    if (_registeredService && _registeredService !== service) {
      if (typeof _registeredService.setBluetoothPanelOpen === "function")
        _registeredService.setBluetoothPanelOpen(root, false)
      _registeredService = null
    }
    if (service && typeof service.setBluetoothPanelOpen === "function") {
      service.setBluetoothPanelOpen(root, root.opened)
      _registeredService = service
    }
  }
  onOpenedChanged: {
    if (!opened) cancelLightingEdit()
    _updateBluetoothPanelOpen()
  }
  onServiceChanged: _updateBluetoothPanelOpen()
  Component.onCompleted: _updateBluetoothPanelOpen()
  Component.onDestruction: {
    if (_registeredService && typeof _registeredService.setBluetoothPanelOpen === "function") {
      _registeredService.setBluetoothPanelOpen(root, false)
      _registeredService = null
    }
  }

  function refreshBluetoothTelemetry() {
    if (service && typeof service.refreshBluetoothTelemetry === "function")
      service.refreshBluetoothTelemetry()
  }
  function bluetoothTelemetryStatusText() {
    if (bluetoothTelemetryState === "loading")
      return root.tr("bluetooth.statusLoading", "Loading telemetry…")
    if (bluetoothTelemetryState === "ready")
      return root.tr("bluetooth.statusReady", "Telemetry ready.")
    if (bluetoothTelemetryState === "stale")
      return root.tr("bluetooth.statusStale", "Telemetry is stale.")
    if (bluetoothTelemetryState === "unavailable")
      return root.tr("bluetooth.statusUnavailable", "Telemetry unavailable. Retrying automatically.")
    return ""
  }
  function bluetoothAncText() {
    return root.tr("bluetooth.ancMode", "Bluetooth ANC: {mode}", { mode: root.modeText(root.bluetoothTelemetryMode) })
  }
  function batterySourceText() {
    if (connected) return root.tr("battery.sourceUsb", "Source: USB")
    if (usesBluetoothTelemetry) return root.tr("battery.sourceBluetooth", "Source: Bluetooth")
    return root.tr("battery.sourceUnknown", "Source: Unknown")
  }
  function caseFreshnessText() {
    return root.tr("battery.caseFreshnessUnknown", "Case charge: last reported; physical freshness unknown.")
  }
  readonly property var modeOptions: [
    { value: "off", label: root.modeText("off"), shortcut: "O" },
    { value: "anc", label: root.modeText("anc"), shortcut: "N" },
    { value: "ambient", label: root.modeText("ambient"), shortcut: "A" }
  ]
  readonly property string currentLocaleCode: {
    var loc = preference("locale", "system")
    return typeof loc === "string" && loc.trim() !== "" ? loc.trim() : "system"
  }
  readonly property string displayLocaleCode: {
    if (root.currentLocaleCode === "system" || root.currentLocaleCode === "auto")
      return i18n.effectiveLocale.toUpperCase().slice(0, 2)
    return root.currentLocaleCode.toUpperCase().slice(0, 2)
  }
  readonly property var languageOptions: [
    { code: "system", label: root.tr("language.system", "System") },
    { code: "en", label: root.tr("language.en", "English") },
    { code: "zh", label: root.tr("language.zh", "简体中文") },
    { code: "es", label: root.tr("language.es", "Español") },
    { code: "ru", label: root.tr("language.ru", "Русский") },
    { code: "pt", label: root.tr("language.pt", "Português") },
    { code: "fr", label: root.tr("language.fr", "Français") },
    { code: "de", label: root.tr("language.de", "Deutsch") },
    { code: "ja", label: root.tr("language.ja", "日本語") },
    { code: "ko", label: root.tr("language.ko", "한국어") },
    { code: "it", label: root.tr("language.it", "Italiano") }
  ]
  readonly property string statusLabel: {
    if (deviceStatus === "starting") return root.tr("status.starting", "Starting device helper")
    if (deviceStatus === "helper-error" || deviceStatus === "waiting") return root.tr("status.helperError", "Device helper stopped. Retrying...")
    if (deviceStatus === "helper-missing") return root.tr("status.helperMissing", "Device helper is not installed")
    if (deviceStatus === "permission-denied") return root.tr("status.permissionDenied", "No permission to read the receiver")
    if (deviceStatus === "protocol-error") return root.tr("status.protocolError", "Unsupported receiver response")
    if (deviceStatus === "timeout" || deviceStatus === "busy") return root.tr("status.waiting", "Waiting for receiver data")
    if (!receiver && bluetoothAudioConnected === true) return root.tr("connection.bluetoothConnected", "Connected via Bluetooth")
    if (!receiver) return root.tr("status.receiverMissing", "USB receiver is not connected")
    if (leftPresent === false && rightPresent === false) return root.tr("status.earbudsUnavailable", "Both earbuds report unavailable")
    if (presenceObserved && leftPresent === null && rightPresent === null) return root.tr("status.presenceUnknown", "Earbud presence is unknown")
    if (!connected) return root.tr("status.telemetryUnavailable", "Earbud telemetry is unavailable")
    return leftPresent === true || rightPresent === true ? root.tr("status.presenceConfirmed", "Connected")
      : root.tr("status.batteryOnly", "Battery telemetry available / presence unknown")
  }

  function modeText(mode) {
    var labels = { off: root.tr("noise.off", "Off"), anc: root.tr("noise.anc", "ANC"), ambient: root.tr("noise.ambient", "Ambient") }
    return Object.prototype.hasOwnProperty.call(labels, mode) ? labels[mode] : root.tr("noise.unknown", "Unknown")
  }
  function lightingText(effect) {
    var labels = {
      off: root.tr("lighting.off", "Off"), cycle: root.tr("lighting.cycle", "Color Cycle"),
      static: root.tr("lighting.static", "Static"), breathing: root.tr("lighting.breathing", "Breathing"), strobing: root.tr("lighting.strobing", "Strobing")
    }
    return Object.prototype.hasOwnProperty.call(labels, effect) ? labels[effect] : root.tr("lighting.unknown", "Unknown")
  }
  function levelText(value) {
    return value === null || value === undefined ? root.tr("battery.noData", "No data")
      : root.tr("battery.percentage", "{value}%", { value: Number(value) })
  }
  function reportText(present, charging, isCase, inline) {
    var power = charging === true ? root.tr("report.charging", "Charging reported")
      : charging === false ? root.tr("report.notCharging", "Not charging") : root.tr("report.chargeUnknown", "Charging state unknown")
    if (isCase) return power
    var availability = present === true ? root.tr("report.present", "Present")
      : present === false ? root.tr("report.unavailable", "Unavailable") : root.tr("report.presenceUnknown", "Presence unknown")
    return inline ? root.tr("report.inline", "{availability} / {power}", { availability: availability, power: power })
      : root.tr("report.lines", "{availability}\n{power}", { availability: availability, power: power })
  }
  function batteryStatusText(present, charging, isCase) {
    if (charging === true) return root.tr("report.chargingCompact", "Charging")
    if (isCase) return charging === false ? root.tr("report.notCharging", "Not charging") : ""
    if (present === false) return root.tr("report.unavailable", "Unavailable")
    return present === true ? "" : root.tr("report.noLiveStatus", "No live status")
  }
  function setListeningMode(mode) {
    if (service) service.setListeningMode(mode)
  }
  function setAncLevel(level) {
    if (service) service.setAncLevel(level)
  }
  function setAncAdaptive(enabled) {
    if (service) service.setAncAdaptive(enabled)
  }
  function chooseAncLevel(level) {
    if (!opened || !connected || listeningMode !== "anc" || [1, 2, 3].indexOf(level) < 0
        || pendingSettings.anc_adaptive !== undefined || pendingSettings.anc_level !== undefined) return
    if (ancAdaptive !== false) setAncAdaptive(false)
    setAncLevel(level)
  }
  function setVoicePrompt(val) {
    if (service) service.setVoicePrompt(val)
  }
  function setLighting(effect) {
    var sent = service ? service.setLighting(effect, root.selectedLightingColor) : false
    lightingFeedback = sent ? "sent" : "rejected"
    return sent
  }
  function applyLightingColor() {
    return setLighting(root.colorApplyEffect)
  }
  function beginLightingEdit() {
    if (!opened || !connected || panelPage !== "device" || devicePage !== "color") return
    var color = selectedLightingColor
    lightingDraftRgb = ["r", "g", "b"].map(function (channel) {
      var value = color ? color[channel] : 1
      return typeof value === "number" && Number.isFinite(value) ? Math.round(Math.max(0, Math.min(1, value)) * 255) : 255
    })
    lightingDraftError = false
    lightingFeedback = ""
    lightingColorExpanded = true
  }
  function setLightingDraftChannel(index, value) {
    if (!lightingColorExpanded || !connected || [0, 1, 2].indexOf(index) < 0
        || typeof value !== "number" || !Number.isFinite(value) || value < 0 || value > 255 || Math.floor(value) !== value) return
    var next = lightingDraftRgb.slice()
    next[index] = value
    lightingDraftRgb = next
    lightingDraftError = false
    lightingFeedback = ""
  }
  function cancelLightingEdit() {
    lightingColorExpanded = false
    lightingDraftError = false
  }
  function commitLightingEdit() {
    if (!opened || !connected || panelPage !== "device" || devicePage !== "color" || !lightingColorExpanded) return false
    var saved = service && typeof service.updateLightingColor === "function"
      && service.updateLightingColor(lightingDraftRgb, settings)
    lightingDraftError = !saved
    if (!saved) return false
    // Use the accepted draft directly: host settings bindings may settle later.
    var sent = service.setLighting(colorApplyEffect, {
      r: lightingDraftRgb[0] / 255, g: lightingDraftRgb[1] / 255, b: lightingDraftRgb[2] / 255
    })
    lightingFeedback = sent ? "sent" : "rejected"
    if (sent) lightingColorExpanded = false
    return !!sent
  }
  function setLightingSetting(name, value) {
    if (!bar || !bar.shell || typeof bar.shell.updateEntryInline !== "function") return false
    if (name === "useThemeColor") {
      if (typeof value !== "boolean") return false
    } else if (["lightingRed", "lightingGreen", "lightingBlue"].indexOf(name) < 0
        || typeof value !== "number" || !isFinite(value)
        || Math.floor(value) !== value || value < 0 || value > 255) return false
    return persistSetting(name, value)
  }
  function cycleListeningMode() {
    if (!connected || pendingMode !== "") return
    if (listeningMode === "off") setListeningMode("anc")
    else if (listeningMode === "anc") setListeningMode("ambient")
    else setListeningMode("off")
  }
  function setLocaleSetting(code) {
    if (typeof code !== "string" || !bar || !bar.shell || typeof bar.shell.updateEntryInline !== "function") return false
    return persistSetting("locale", code)
  }
  function setAutoThemeColor(enabled) {
    return service ? service.setAutoThemeColor(enabled) : false
  }
  function setShowMicLevel(enabled) {
    if (typeof enabled !== "boolean" || !bar || !bar.shell || typeof bar.shell.updateEntryInline !== "function") return false
    return persistSetting("showMicLevel", enabled)
  }
  function microphoneHelpText() {
    var info = root.tr("microphone.usbOnly", "Signal metering requires USB. Follow the headset voice prompt for native mute.")
      + "\n" + root.tr("microphone.unknown", "Microphone mute: unknown")
    if (!connected) return info
    return info + "\n" + (root.callContextActive
      ? root.tr("microphone.callGesture", "Call mode requested. Follow the headset voice prompt; tap behavior is not confirmed.")
      : root.tr("microphone.mediaGesture", "Call mode not requested. A tap may control playback."))
  }
  function microphoneSignalState() {
    if (!connected) return "usb-only"
    if (!showMicLevel) return "disabled"
    if (microphoneCaptureState === "inactive") return "idle"
    if (microphoneCaptureState !== "active") return "unavailable"
    if (typeof microphoneLevel !== "number" || !Number.isFinite(microphoneLevel) || microphoneLevel < 0) return "waiting"
    return microphoneLevel === 0 ? "silent" : "signal"
  }
  function microphoneLevelText() {
    var state = microphoneSignalState()
    if (state === "usb-only") return root.tr("microphone.levelUsb", "Signal level requires USB")
    if (state === "disabled") return root.tr("microphone.levelDisabled", "Signal indicator disabled")
    if (state === "idle") return root.tr("microphone.levelIdle", "No active recording")
    if (state === "unavailable") return root.tr("microphone.levelUnavailable", "Mic level: no capture data")
    if (state === "waiting") return root.tr("microphone.levelWaiting", "Waiting for signal data")
    if (state === "silent") return root.tr("microphone.levelSilent", "No signal · 0%")
    return root.tr("microphone.levelSignal", "Signal: {value}%", { value: Math.round(Math.min(1, microphoneLevel) * 100) })
  }

  function persistSetting(name, value) {
    if (service && typeof service.updateSetting === "function") return service.updateSetting(name, value, settings)
    var entry = Object.assign({}, settings || {}, { id: root.moduleName })
    entry[name] = value
    return bar.shell.updateEntryInline(root.moduleName, entry)
  }
  function settingStateText(value) {
    return value === true ? root.tr("settings.on", "On") : value === false ? root.tr("settings.off", "Off") : root.tr("settings.unknown", "Unknown")
  }
  function settingsFeedback() {
    return settingsStatusKey === "settings.notConfirmed" ? root.tr("settings.notConfirmed", "Setting change not confirmed. Try again.")
      : settingsStatusKey === "settings.pending" ? root.tr("settings.pending", "Waiting for setting readback...") : ""
  }
}
