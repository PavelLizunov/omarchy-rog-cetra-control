import QtQuick
import QtQuick.Effects
import Quickshell

Item {
  id: root

  property color color: "white"
  property real iconSize: Math.min(width, height)

  implicitWidth: iconSize
  implicitHeight: iconSize

  Image {
    id: sourceImage
    anchors.centerIn: parent
    width: root.iconSize
    height: root.iconSize
    source: Qt.resolvedUrl("assets/cetra-symbolic.svg")
    sourceSize.width: Math.round(width * Screen.devicePixelRatio)
    sourceSize.height: Math.round(height * Screen.devicePixelRatio)
    fillMode: Image.PreserveAspectFit
    visible: false
    layer.enabled: true
  }

  MultiEffect {
    anchors.fill: sourceImage
    source: sourceImage
    colorization: 1.0
    colorizationColor: root.color
  }
}
