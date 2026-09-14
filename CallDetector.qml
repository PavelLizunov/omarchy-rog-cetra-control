import QtQuick

// Event-driven admission, with bounded delayed confirmation of lost capture.
Item {
  id: detector
  required property var root
  required property string observation
  property int confirmations: 0
  onObservationChanged: publish()
  Component.onCompleted: publish()

  function publish() {
    settle.stop()
    confirmations = 0
    root.applyCallContext(observation)
    if (observation !== "active") settle.start()
  }

  Timer {
    id: settle
    interval: 2000
    onTriggered: {
      detector.root.applyCallContext(detector.observation)
      detector.confirmations++
      if (detector.confirmations < 2 && detector.observation !== "active") restart()
    }
  }
}
