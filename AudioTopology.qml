import QtQuick
import Quickshell.Services.Pipewire

// One service-owned observer. Traversal follows actual links, never target hints.
Item {
  id: topology
  required property bool active
  readonly property var nodes: active ? Pipewire.nodes.values : []
  readonly property var links: active ? Pipewire.links.values : []
  readonly property bool withinBudget: nodes.length <= 512 && links.length <= 2048
  readonly property var source: selectSource(nodes)
  readonly property var observation: active && Pipewire.ready && withinBudget
    ? observe(source, nodes, links) : ({ capture: "unknown", communication: "unknown" })
  PwObjectTracker {
    objects: topology.active && topology.withinBudget ? topology.nodes.concat(topology.links) : []
  }

  function selectSource(values) {
    var matches = values.filter(function (node) {
      return node && node.isStream === false && node.isSink === false
        && (node.type & PwNodeType.AudioSource) === PwNodeType.AudioSource
        && /^alsa_input\.usb-ASUSTek_ROG_CETRA_TRUE_WIRELESS_SPEEDNOVA_[^.]+\./.test(node.name)
    })
    return matches.length === 1 ? matches[0] : null
  }

  function identity(node) {
    var p = node.properties || {}
    return [node.name, p["application.name"], p["application.id"], p["application.process.binary"],
      p["pipewire.access.portal.app_id"], p["media.name"], p["media.filename"]].join(" ")
  }

  function isMonitor(node) {
    if (!node) return true
    var p = node.properties || {}
    return p["application.id"] === "io.github.pavellizunov.rog-cetra-control.peak"
      || /quickshell[ _-]peak|peak detect/i.test(identity(node))
      || p["media.category"] === "Monitor" || p["stream.monitor"] === true || p["stream.monitor"] === "true"
  }

  function isEndpoint(node) {
    var p = node.properties || {}
    return node.isStream === true && node.isSink === false && !isMonitor(node)
      && !/easy[ _-]?effects|keepalive|\/dev\/null|voxtype|recognition/i.test(identity(node))
      && !/^(DSP|Filter)$/i.test(p["media.role"] || "")
      && p["pulse.corked"] !== true && p["pulse.corked"] !== "true"
  }

  function isCommunication(node) {
    var p = node.properties || {}
    var role = p["media.role"] || ""
    // Explicit non-call roles take precedence over application-name heuristics.
    if (role) return /^(phone|communication)$/i.test(role)
    return /(^|[^a-z0-9_])(webrtc|discord|vesktop|steam(webhelper)?|telegram|zoom)([^a-z0-9_]|$)/i.test(identity(node))
  }

  function observe(mic, values, edges) {
    if (!mic || values.length > 512 || edges.length > 2048)
      return { capture: "unknown", communication: "unknown" }
    var incoming = new Map()
    var uncertain = !mic.ready
    for (var i = 0; i < edges.length; i++) {
      var edge = edges[i]
      if (!edge.source || !edge.target) { uncertain = true; continue }
      if (edge.state !== PwLinkState.Active) continue
      if (!incoming.has(edge.target)) incoming.set(edge.target, [])
      incoming.get(edge.target).push(edge.source)
    }
    var capture = false, communication = false
    for (var n = 0; n < values.length; n++) {
      var endpoint = values[n]
      if (!endpoint.ready) { uncertain = true; continue }
      if (!isEndpoint(endpoint)) continue
      var queue = [endpoint], visited = new Set([endpoint]), reachesMic = false, otherInput = false, incomplete = false
      for (var pos = 0; pos < queue.length; pos++) {
        var node = queue[pos]
        if (visited.size > 512) { incomplete = true; break }
        if (!node.ready) { incomplete = true; continue }
        if (node === mic) { reachesMic = true; continue }
        // A second physical source makes processed audio attribution ambiguous.
        if (!node.isStream && /^alsa_input\./.test(node.name)) { otherInput = true; continue }
        var parents = incoming.get(node) || []
        for (var j = 0; j < parents.length; j++)
          if (!visited.has(parents[j])) { visited.add(parents[j]); queue.push(parents[j]) }
      }
      if (reachesMic && !otherInput && !incomplete) {
        capture = true
        if (isCommunication(endpoint)) communication = true
      } else if (reachesMic || incomplete) uncertain = true
    }
    return { capture: capture ? "active" : uncertain ? "unknown" : "inactive",
      communication: communication ? "active" : uncertain ? "unknown" : "inactive" }
  }
}
