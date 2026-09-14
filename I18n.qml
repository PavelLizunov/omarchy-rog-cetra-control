pragma ComponentBehavior: Bound

import QtQml
import Quickshell.Io

QtObject {
  id: root

  property string language: "system"
  readonly property string locale: resolveLocale(language, Qt.locale().name)
  readonly property var availableLocales: _registry
  readonly property string effectiveLocale: effectiveLanguage()
  readonly property bool rightToLeft: {
    var entry = _registry[effectiveLocale]
    return entry !== undefined && entry.direction === "rtl"
  }

  property var _registry: ({ en: { file: "en.json", name: "English", direction: "ltr" } })
  property var _english: ({})
  property var _selection: ({ locale: "", generation: 0, catalogs: {} })
  property var _readers: []
  property int _generation: 0
  property bool _ready: false

  onLocaleChanged: reloadSelection()
  Component.onCompleted: {
    _ready = true
    reloadSelection()
  }

  function normalizeLocale(value) {
    if (typeof value !== "string")
      return ""
    var tag = value.trim().replace(/_/g, "-")
    // Modern BCP 47 language tags; reject paths, POSIX suffixes and malformed subtags.
    if (!/^(?:[A-Za-z]{2,3}(?:-[A-Za-z]{3}){0,3}|[A-Za-z]{4,8})(?:-[A-Za-z]{4})?(?:-(?:[A-Za-z]{2}|[0-9]{3}))?(?:-(?:[A-Za-z0-9]{5,8}|[0-9][A-Za-z0-9]{3}))*(?:-[0-9A-WY-Za-wy-z](?:-[A-Za-z0-9]{2,8})+)*(?:-[xX](?:-[A-Za-z0-9]{1,8})+)?$/.test(tag))
      return ""
    var parts = tag.toLowerCase().split("-")
    for (var i = 1; i < parts.length; ++i) {
      if (parts[i].length === 1)
        break
      if (/^[a-z]{4}$/.test(parts[i]))
        parts[i] = parts[i][0].toUpperCase() + parts[i].slice(1)
      else if (/^[a-z]{2}$/.test(parts[i]))
        parts[i] = parts[i].toUpperCase()
    }
    return parts.join("-")
  }

  function resolveLocale(selection, systemLocale) {
    var choice = typeof selection === "string" ? selection.trim().toLowerCase() : ""
    return normalizeLocale(choice === "system" || choice === "auto" || choice === "" ? systemLocale : selection) || "en"
  }

  function localeChain(code) {
    var exact = normalizeLocale(code) || "en"
    var base = exact.split("-")[0]
    var chain = [exact]
    // The bundled zh catalog is Simplified. Explicit script wins over region.
    var script = exact.match(/-([A-Z][a-z]{3})(?:-|$)/)
    var traditional = base === "zh" && (script ? script[1] === "Hant" : /-(TW|HK|MO)(?:-|$)/.test(exact))
    if (traditional && exact !== "zh-Hant")
      chain.push("zh-Hant")
    if (base !== exact && !traditional)
      chain.push(base)
    if (base !== "en")
      chain.push("en")
    return chain
  }

  function parseObject(source) {
    try {
      var data = JSON.parse(source)
      return data && typeof data === "object" && !Array.isArray(data) ? data : {}
    } catch (error) {
      return {}
    }
  }

  function parseRegistry(source) {
    var data = parseObject(source)
    var registry = Object.create(null)
    Object.keys(data).forEach(function (key) {
      var code = normalizeLocale(key)
      var entry = data[key]
      if (!code || code !== key || code === "en" || !entry || typeof entry !== "object"
          || typeof entry.file !== "string" || entry.file !== entry.file.trim() || !/^[A-Za-z0-9-]+\.json$/.test(entry.file)
          || typeof entry.name !== "string" || entry.name.trim() === ""
          || (entry.direction !== "ltr" && entry.direction !== "rtl"))
        return
      registry[code] = { file: entry.file, name: entry.name, direction: entry.direction }
    })
    // English must remain available even when the index is missing or invalid.
    registry.en = { file: "en.json", name: "English", direction: "ltr" }
    return registry
  }

  function parseCatalog(source) {
    var data = parseObject(source)
    var catalog = Object.create(null)
    Object.keys(data).forEach(function (key) {
      if (key === key.trim() && /^[a-z][a-zA-Z0-9]*(?:\.[a-zA-Z0-9]+)*$/.test(key)
          && key !== "constructor" && key !== "prototype"
          && typeof data[key] === "string" && data[key].trim() !== "")
        catalog[key] = data[key]
    })
    return catalog
  }

  function localPath(file) {
    if (typeof file !== "string" || file !== file.trim() || !/^[A-Za-z0-9-]+\.json$/.test(file))
      return ""
    var url = Qt.resolvedUrl("locales/" + file).toString()
    // FileView strips file:// but does not URL-decode paths in Quickshell 0.3.1.
    return url.indexOf("file:///") === 0 ? decodeURIComponent(url.slice(7)) : ""
  }

  function reloadSelection() {
    if (!_ready)
      return
    var generation = ++_generation
    _selection = { locale: locale, generation: generation, catalogs: {} }
    _readers.forEach(function (reader) { reader.destroy() })
    _readers = []
    var chain = localeChain(locale)
    var readers = []
    for (var i = 0; i < chain.length; ++i) {
      var code = chain[i]
      if (code === "en" || !Object.prototype.hasOwnProperty.call(_registry, code))
        continue
      var reader = _catalogReader.createObject(root, {
        request: { locale: locale, code: code, generation: generation, file: _registry[code].file }
      })
      if (reader)
        readers.push(reader)
    }
    _readers = readers
  }

  function acceptCatalog(request, source) {
    if (request.generation !== _selection.generation || request.locale !== locale
        || _selection.locale !== locale)
      return
    var catalogs = Object.assign({}, _selection.catalogs)
    catalogs[request.code] = parseCatalog(source)
    _selection = { locale: locale, generation: request.generation, catalogs: catalogs }
  }

  function effectiveLanguage() {
    var chain = localeChain(locale)
    if (_selection.locale === locale) {
      for (var i = 0; i < chain.length; ++i) {
        var catalog = _selection.catalogs[chain[i]]
        if (catalog && Object.prototype.hasOwnProperty.call(_registry, chain[i]) && Object.keys(catalog).length > 0)
          return chain[i]
      }
    }
    return "en"
  }

  function text(key, fallback, values) {
    var result = typeof fallback === "string" ? fallback : String(key)
    var chain = localeChain(locale)
    for (var i = 0; i < chain.length; ++i) {
      var code = chain[i]
      var catalog = code === "en" ? _english : _selection.locale === locale ? _selection.catalogs[code] : null
      if (catalog && Object.prototype.hasOwnProperty.call(catalog, key)) {
        result = catalog[key]
        break
      }
    }
    // Callback replacement keeps dollar signs literal and never evaluates values.
    return result.replace(/\{\{|\}\}|\{([A-Za-z_][A-Za-z0-9_]*)\}/g, function (match, name) {
      if (match === "{{")
        return "{"
      if (match === "}}")
        return "}"
      if (!values || !Object.prototype.hasOwnProperty.call(values, name))
        return match
      var value = values[name]
      return typeof value === "string" || typeof value === "number" || typeof value === "boolean" ? String(value) : match
    })
  }

  property FileView _indexView: FileView {
    path: root.localPath("index.json")
    blockLoading: false
    blockAllReads: false
    printErrors: false
    onLoaded: {
      root._registry = root.parseRegistry(text())
      root.reloadSelection()
    }
    onLoadFailed: {
      root._registry = root.parseRegistry("")
      root.reloadSelection()
    }
  }

  property FileView _englishView: FileView {
    path: root.localPath("en.json")
    blockLoading: false
    blockAllReads: false
    printErrors: false
    onLoaded: root._english = root.parseCatalog(text())
    onLoadFailed: root._english = ({})
  }

  property Component _catalogReader: Component {
    FileView {
      id: reader
      required property var request
      blockLoading: false
      blockAllReads: false
      printErrors: false
      // Set the path only after immutable request metadata has been initialized.
      Component.onCompleted: path = root.localPath(request.file)
      onLoaded: root.acceptCatalog(reader.request, reader.text())
      onLoadFailed: root.acceptCatalog(reader.request, "")
    }
  }
}
