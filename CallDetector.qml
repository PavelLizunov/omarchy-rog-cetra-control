import QtQuick
import Quickshell.Io

// One bounded metadata probe, owned by CetraService (not individual views).
Item {
  id: detector
  required property var root
  property alias process: callContextProc
  property alias timeout: callContextTimeout
  Process {
    id: callContextProc
    property bool pending: false
    command: ["timeout", "--signal=KILL", "2s", "bash", "-c", "set -o pipefail; pactl -f json list source-outputs | jq -r 'any(.[]; . as $s | (.properties // {}) as $p | ([\"application.name\", \"application.process.binary\", \"application.id\", \"application.icon_name\", \"pipewire.access.portal.app_id\", \"node.name\", \"media.name\", \"media.filename\"] | map(($p[.] // \"\") | tostring) | join(\" \")) as $id | ($s.corked != true) and (($id | test(\"easy[ _-]?effects|pw-(record|cat)|voxtype|recognition|keepalive|/dev/null\"; \"i\") | not) and (($id | test(\"(^|[^[:alnum:]_])(webrtc|chrom(e|ium)( input)?|firefox|discord|vesktop|steam(webhelper)?|telegram|zoom|brave|vivaldi|microsoft-edge)([^[:alnum:]_]|$)\"; \"i\")) or (($p[\"media.role\"] // \"\") | test(\"^(phone|communication)$\"; \"i\"))))) | if . then \"active\" else \"inactive\" end'"]
    stdout: StdioCollector { id: callContextOutput; waitForEnd: true }
    onExited: function (exitCode, exitStatus) {
      root.finishCallContext(exitCode === 0 && exitStatus === 0 ? callContextOutput.text : "unknown")
    }
    onRunningChanged: {
      if (!callContextProc.running) root.finishCallContext("unknown")
    }
  }
  Timer {
    id: callContextTimeout
    interval: 4000
    onTriggered: {
      root.finishCallContext("unknown")
      if (callContextProc.running && callContextProc.processId > 0) callContextProc.signal(14)
    }
  }
  Timer {
    id: callContextPoll
    interval: 2000
    running: root.hostReady && root.receiver
    repeat: true
    triggeredOnStart: true
    onTriggered: {
      if (!root.hostReady || callContextProc.running) return
      callContextProc.pending = true
      callContextTimeout.restart()
      callContextProc.running = true
    }
  }
  Component.onDestruction: {
    // Best effort, not proof of awaited descendant cleanup.
    if (callContextProc.running && callContextProc.processId > 0) callContextProc.signal(14)
  }
}
