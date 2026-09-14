import QtQuick
import QtQuick.Effects
import qs.Commons

Item {
  id: root

  property string name: "cetra"
  property color color: Color.foreground
  property real iconSize: 24

  implicitWidth: iconSize
  implicitHeight: iconSize

  Image {
    id: sourceImage
    anchors.centerIn: parent
    width: Math.max(0, Math.min(root.width, root.height, root.iconSize))
    height: width
    source: Qt.resolvedUrl("assets/" + root.name + "-symbolic.svg")
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
