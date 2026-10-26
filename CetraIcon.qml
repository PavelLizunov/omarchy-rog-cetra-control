import QtQuick
import QtQuick.Effects
import qs.Commons
import qs.Commons as Host

Item {
  id: root

  property string name: "cetra"
  property color color: Host.Color.foreground
  property real iconSize: 24

  implicitWidth: iconSize
  implicitHeight: iconSize

  Image {
    id: sourceImage
    anchors.centerIn: parent
    width: Math.max(0, Math.min(root.width, root.height, root.iconSize))
    height: width
    source: Qt.resolvedUrl("assets/" + root.name + "-symbolic.svg")
    sourceSize.width: Math.ceil(width * Screen.devicePixelRatio * 4)
    sourceSize.height: Math.ceil(height * Screen.devicePixelRatio * 4)
    fillMode: Image.PreserveAspectFit
    smooth: true
    mipmap: true
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
