pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes
import qs.Commons

Item {
  id: indicator
  required property var root

  readonly property real scaleFactor: Style.bar.iconFont / 13
  implicitWidth: 27 * scaleFactor
  implicitHeight: 26 * scaleFactor
  width: implicitWidth
  height: implicitHeight

  readonly property string leftEarbudPath: indicator.leftOutline + " M11.2 23.1 Q11.5 22.4 12.2 22.8 L18.2 25.6 Q18.9 25.9 18.6 26.6 Q18.3 27.3 17.6 27 L11.6 24.2 Q10.9 23.8 11.2 23.1 Z M22.5 17.7 Q23 17.3 23.5 17.8 Q25 21.1 23.8 24 Q23.5 24.8 22.8 24.5 Q22.1 24.2 22.4 23.5 Q23.2 21.1 22.2 18.7 Q21.9 18 22.5 17.7 Z"
  readonly property string rightEarbudPath: indicator.rightOutline + " M 52.8 23.1 Q 52.5 22.4 51.8 22.8 L 45.8 25.6 Q 45.1 25.9 45.4 26.6 Q 45.7 27.3 46.4 27 L 52.4 24.2 Q 53.1 23.8 52.8 23.1 Z M 41.5 17.7 Q 41 17.3 40.5 17.8 Q 39 21.1 40.2 24 Q 40.5 24.8 41.2 24.5 Q 41.9 24.2 41.6 23.5 Q 40.8 21.1 41.8 18.7 Q 42.1 18 41.5 17.7 Z"
  readonly property string leftOutline: "M15.8 8 C9.6 8 5.4 12.4 5.4 18.1 C5.4 21.6 7.2 24 10.3 26.2 L5.9 48.9 Q5.5 50.9 7.2 51.7 L12.2 54.1 Q14.1 55 14.5 52.9 L19.5 30.1 Q19.9 28.2 21.4 26.5 L23.5 24.8 C26.5 25.8 28.6 23.6 28.6 21 C28.6 18.3 26.5 16.1 23.8 16.9 C23.1 11.6 20.2 8 15.8 8 Z"
  readonly property string rightOutline: "M 48.2 8 C 54.4 8 58.6 12.4 58.6 18.1 C 58.6 21.6 56.8 24 53.7 26.2 L 58.1 48.9 Q 58.5 50.9 56.8 51.7 L 51.8 54.1 Q 49.9 55 49.5 52.9 L 44.5 30.1 Q 44.1 28.2 42.6 26.5 L 40.5 24.8 C 37.5 25.8 35.4 23.6 35.4 21 C 35.4 18.3 37.5 16.1 40.2 16.9 C 40.9 11.6 43.8 8 48.2 8 Z"
  readonly property string micPath: "M24 6.5 C24.83 6.5 25.5 7.17 25.5 8 V18 C25.5 18.83 24.83 19.5 24 19.5 C23.17 19.5 22.5 18.83 22.5 18 V8 C22.5 7.17 23.17 6.5 24 6.5 Z"

  function isValidCharge(val) {
    return typeof val === "number" && Number.isFinite(val) && val >= 0 && val <= 100
  }

  readonly property bool hasLeftLevel: indicator.isValidCharge(root.leftLevel)
  readonly property real leftPct: indicator.hasLeftLevel ? root.leftLevel : 0
  readonly property real leftFillHeight: 47 * (indicator.leftPct / 100)
  readonly property real leftFillY: 55 - indicator.leftFillHeight

  readonly property bool hasRightLevel: indicator.isValidCharge(root.rightLevel)
  readonly property real rightPct: indicator.hasRightLevel ? root.rightLevel : 0
  readonly property real rightFillHeight: 47 * (indicator.rightPct / 100)
  readonly property real rightFillY: 55 - indicator.rightFillHeight

  readonly property bool isMicSignalValid: root.connected && root.microphoneCaptureState === "active"
    && typeof root.microphoneLevel === "number" && Number.isFinite(root.microphoneLevel) && root.microphoneLevel >= 0
  readonly property real micFraction: indicator.isMicSignalValid ? Math.max(0, Math.min(1, root.microphoneLevel)) : 0
  readonly property real micVisualFraction: Math.sqrt(indicator.micFraction)
  readonly property real micFillHeight: 13 * indicator.micVisualFraction
  readonly property real micFillOpacity: 1.0
  readonly property bool micFillVisible: indicator.isMicSignalValid && indicator.micFraction > 0
  readonly property real chargeFillEdgeOverlap: 2
  readonly property color chargeShellColor: Qt.rgba(root.barColor.r, root.barColor.g, root.barColor.b, 0.28)

  Item {
    id: canvas
    layer.enabled: true
    layer.textureSize: Qt.size(width * 4, height * 4)
    layer.smooth: true
    layer.mipmap: true
    x: 2.5 * indicator.scaleFactor
    y: 2 * indicator.scaleFactor
    width: 27
    height: 26
    scale: indicator.scaleFactor * 0.85
    transformOrigin: Item.TopLeft

    // Rounded sound chambers, inward tips and sloped Cetra stems.
    Item {
      id: earbudArt
      x: 2.5
      y: 3.75
      width: 64
      height: 64
      scale: 19 / 64
      transformOrigin: Item.TopLeft

      // A subdued shell keeps the body legible above the charge waterline.
      // Measured zero stays hollow; presence keeps the shell when charge is unknown.
      Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
          strokeWidth: 0
          strokeColor: "transparent"
          fillColor: (indicator.hasLeftLevel ? indicator.leftPct > 0 : root.leftPresent === true)
            ? indicator.chargeShellColor : "transparent"
          PathSvg { path: indicator.leftOutline }
        }
        ShapePath {
          strokeWidth: 0
          strokeColor: "transparent"
          fillColor: (indicator.hasRightLevel ? indicator.rightPct > 0 : root.rightPresent === true)
            ? indicator.chargeShellColor : "transparent"
          PathSvg { path: indicator.rightOutline }
        }
      }

      // Left earbud dynamic charge fill (bottom-to-top waterline y=55..8)
      Item {
        id: leftFillClip
        x: 0
        y: indicator.leftFillY - indicator.chargeFillEdgeOverlap
        width: 32
        height: indicator.leftFillHeight + indicator.chargeFillEdgeOverlap
        clip: true
        visible: indicator.hasLeftLevel && indicator.leftPct > 0

        Shape {
          x: 0
          y: -leftFillClip.y
          preferredRendererType: Shape.CurveRenderer
          ShapePath {
            strokeWidth: 0
            strokeColor: "transparent"
            fillRule: ShapePath.OddEvenFill
            fillColor: root.barColor
            PathSvg { path: indicator.leftEarbudPath }
          }
        }
      }

      // Right earbud dynamic charge fill (bottom-to-top waterline y=55..8)
      Item {
        id: rightFillClip
        x: 32
        y: indicator.rightFillY - indicator.chargeFillEdgeOverlap
        width: 32
        height: indicator.rightFillHeight + indicator.chargeFillEdgeOverlap
        clip: true
        visible: indicator.hasRightLevel && indicator.rightPct > 0

        Shape {
          x: -32
          y: -rightFillClip.y
          preferredRendererType: Shape.CurveRenderer
          ShapePath {
            strokeWidth: 0
            strokeColor: "transparent"
            fillRule: ShapePath.OddEvenFill
            fillColor: root.barColor
            PathSvg { path: indicator.rightEarbudPath }
          }
        }
      }

      // The outer contour stays fixed as each charge fill changes.
      Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
          strokeWidth: 4
          strokeColor: root.barColor
          fillColor: "transparent"
          joinStyle: ShapePath.RoundJoin
          capStyle: ShapePath.RoundCap
          PathSvg { path: indicator.leftOutline }
        }
        ShapePath {
          strokeWidth: 4
          strokeColor: root.barColor
          fillColor: "transparent"
          joinStyle: ShapePath.RoundJoin
          capStyle: ShapePath.RoundCap
          PathSvg { path: indicator.rightOutline }
        }
      }

      // Small internal unknown dots when charge level is missing/invalid
      Rectangle {
        x: 13.3
        y: 16.5
        width: 5.4
        height: 5.4
        radius: 2.7
        color: root.barColor
        visible: !indicator.hasLeftLevel
      }

      Rectangle {
        x: 45.3
        y: 16.5
        width: 5.4
        height: 5.4
        radius: 2.7
        color: root.barColor
        visible: !indicator.hasRightLevel
      }
    }

    // Upright microphone capsule; only its internal fill follows amplitude.
    Item {
      id: micArt
      anchors.fill: parent
      visible: root.showMicLevel
      Accessible.role: Accessible.StaticText
      Accessible.name: root.showMicLevel ? root.microphoneLevelText() : ""

      // Keep the capsule whole; dim it when capture data is unavailable.
      Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
          strokeWidth: 1
          strokeColor: Qt.rgba(root.barForeground.r, root.barForeground.g, root.barForeground.b, indicator.isMicSignalValid ? 1.0 : 0.55)
          capStyle: ShapePath.RoundCap
          fillColor: "transparent"
          PathSvg { path: indicator.micPath }
        }
      }

      // Bottom-to-top measured signal only; never fabricate a fill for missing data.
      Item {
        id: micFillClip
        x: 22
        y: 19.5 - height
        width: 4
        height: indicator.micFillHeight
        clip: true
        visible: indicator.micFillVisible
        opacity: indicator.micFillOpacity

        Behavior on height {
          NumberAnimation { duration: 90; easing.type: Easing.OutCubic }
        }

        Shape {
          x: -22
          y: -micFillClip.y
          preferredRendererType: Shape.CurveRenderer
          ShapePath {
            strokeWidth: 0
            strokeColor: "transparent"
            fillColor: root.barForeground
            PathSvg { path: indicator.micPath }
          }
        }
      }
    }
  }
}
