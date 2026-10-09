import QtQuick
Item {
 property QtObject bar
 property bool labelVisible
 property bool hasVisualContent
 property bool vertical: false
 property real fixedWidth
 property real fixedHeight
 property real horizontalMargin
 property real verticalPadding
 property string tooltipText
 implicitWidth: 27
 implicitHeight: 26
 signal pressed(int button)
 signal wheelMoved(real delta)
}
