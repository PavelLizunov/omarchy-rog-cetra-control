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
    var config
    try { config = JSON.parse(text) } catch (error) { return }
    if (!config || config.version !== 1 || !manifest) return
    var layout = config.bar && config.bar.layout
    for (var section of ["left", "center", "right"]) {
      var entries = layout && layout[section]
      if (!Array.isArray(entries)) continue
      for (var entry of entries)
        if (entry && entry.id === manifest.id) { syncSettings(entry); return }
    }
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
  FileView {
    id: savedConfig
    path: root.hostReady ? Quickshell.env("HOME") + "/.config/omarchy/shell.json" : ""
    watchChanges: true
    blockLoading: false
    blockAllReads: false
    printErrors: false
    onFileChanged: reload()
    onLoaded: root.applySavedConfig(text())
  }
  Timer {
    id: preferenceReadback
    interval: 3000
    onTriggered: {
      pendingPreferences = ({})
      savedConfig.reload()
    }
  }
}
