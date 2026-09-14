import QtQuick
import Quickshell
import Quickshell.Io

// Host-injected configuration and saved preferences. No HID or audio processes.
Item {
  id: root
  property var shell: null
  property var manifest: null
  property var pluginRegistry: null
  readonly property bool hostReady: shell !== null && manifest !== null && pluginRegistry !== null
  property var inlineSettings: null
  property var pendingPreferences: ({})
  readonly property string settingsCommand: decodeURIComponent(Qt.resolvedUrl("bin/cetra-status").toString().replace("file://", ""))
  property bool settingsReadPending: false
  property int settingsReadGeneration: 0
  onHostReadyChanged: {
    settingsReadGeneration++
    if (hostReady) queueSettingsRead()
    else { settingsReadPending = false; settingsReadDelay.stop(); settingsReader.running = false }
  }
  readonly property var settings: inlineSettings !== null ? inlineSettings : hostSettings
  readonly property var hostSettings: {
    if (!hostReady) return {}
    var layout = shell.barConfig && shell.barConfig.layout
    var sections = ["left", "center", "right"]
    for (var s = 0; layout && s < sections.length; s++) {
      var entries = layout[sections[s]]
      if (!Array.isArray(entries)) continue
      for (var i = 0; i < entries.length; i++)
        if (entries[i] && entries[i].id === manifest.id) return entries[i]
    }
    return {}
  }
  function syncSettings(value) {
    if (!value || typeof value !== "object" || Array.isArray(value)) return
    var pending = {}
    for (var key in pendingPreferences)
      if (value[key] !== pendingPreferences[key]) pending[key] = pendingPreferences[key]
    pendingPreferences = pending
    inlineSettings = Object.assign({}, value, pending)
    if (!Object.keys(pending).length) preferenceReadback.stop()
  }
  function applySavedConfig(text) {
    if (text.length > 1048576) return
    var config
    try { config = JSON.parse(text) } catch (error) { return }
    if (!config || config.version !== 1 || !manifest) return
    var layout = config.bar && config.bar.layout
    if (!layout || typeof layout !== "object" || Array.isArray(layout)) return
    for (var name of ["left", "center", "right"])
      if (layout[name] !== undefined && !Array.isArray(layout[name])) return
    for (var section of ["left", "center", "right"]) {
      var entries = layout && layout[section]
      if (!Array.isArray(entries)) continue
      for (var entry of entries)
        if (entry && entry.id === manifest.id) { syncSettings(entry); return }
    }
    // A valid host document without our entry means removal, not stale settings.
    pendingPreferences = ({})
    inlineSettings = ({})
    preferenceReadback.stop()
  }
  function updateSetting(name, value, fallback) {
    if (!hostReady || typeof shell.updateEntryInline !== "function") return false
    var entry = Object.assign({}, fallback || {}, settings, { id: manifest.id })
    entry[name] = value
    var changed = shell.updateEntryInline(manifest.id, entry)
    if (changed) {
      pendingPreferences = Object.assign({}, pendingPreferences, { [name]: value })
      inlineSettings = entry
      preferenceReadback.restart()
    }
    return changed
  }
  function queueSettingsRead() {
    if (!hostReady) return
    settingsReadPending = true
    settingsReadDelay.restart()
  }
  FileView {
    id: savedConfig
    path: root.hostReady ? Quickshell.env("HOME") + "/.config/omarchy/shell.json" : ""
    watchChanges: true
    preload: false
    blockLoading: false
    blockAllReads: false
    printErrors: false
    onFileChanged: root.queueSettingsRead()
  }
  Process {
    id: settingsReader
    property int generation: -1
    command: [root.settingsCommand, "--read-settings"]
    stdout: StdioCollector { id: settingsOutput; waitForEnd: true }
    onExited: function (exitCode) {
      if (root.hostReady && generation === root.settingsReadGeneration && exitCode === 0)
        root.applySavedConfig(settingsOutput.text)
    }
    onRunningChanged: {
      if (!running && root.settingsReadPending && root.hostReady) settingsReadDelay.restart()
    }
  }
  Timer {
    id: settingsReadDelay
    interval: 100
    onTriggered: {
      if (root.hostReady && root.settingsReadPending && !settingsReader.running) {
        root.settingsReadPending = false
        settingsReader.generation = root.settingsReadGeneration
        settingsReader.running = true
      }
    }
  }
  Timer {
    id: preferenceReadback
    interval: 3000
    onTriggered: {
      root.pendingPreferences = ({})
      root.queueSettingsRead()
    }
  }
}
