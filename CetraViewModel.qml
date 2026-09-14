import QtQuick
import qs.Commons
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
  readonly property color foreground: bar ? bar.foreground : Color.foreground
  readonly property color accent: bar && bar.accent !== undefined ? bar.accent : Color.accent
  readonly property color barColor: lowestLevel >= 0 && lowestLevel <= 20 ? (bar ? bar.urgent : Color.urgent) : barForeground
  readonly property color dim: Qt.rgba(foreground.r, foreground.g, foreground.b, 0.85)
  readonly property color rule: Qt.rgba(foreground.r, foreground.g, foreground.b, 0.12)
  readonly property string fontFamily: bar ? bar.fontFamily : Style.font.family
  property bool settingsExpanded: false
  property bool lightingColorExpanded: false
  property bool languageExpanded: false
  property string lightingFeedback: ""
  readonly property bool showPercentage: preference("showPercentage", true) === true
  readonly property bool hideWhenReceiverMissing: preference("hideWhenReceiverMissing", true) === true
  readonly property string deviceStatus: service ? service.deviceStatus : "starting"
  readonly property bool receiver: service ? service.receiver : false
  readonly property bool connected: service ? service.connected : false
  readonly property var leftPresent: service ? service.leftPresent : null
  readonly property var rightPresent: service ? service.rightPresent : null
  readonly property var leftCharging: service ? service.leftCharging : null
  readonly property var rightCharging: service ? service.rightCharging : null
  readonly property var caseCharging: service ? service.caseCharging : null
  readonly property bool presenceObserved: service ? service.presenceObserved : false
  readonly property var leftLevel: service ? service.leftLevel : null
  readonly property var rightLevel: service ? service.rightLevel : null
  readonly property var caseLevel: service ? service.caseLevel : null
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
    if (leftLevel !== null) levels.push(Number(leftLevel))
    if (rightLevel !== null) levels.push(Number(rightLevel))
    return levels.length ? Math.min.apply(null, levels) : -1
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
