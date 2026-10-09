import QtQuick
import qs.Commons
BorderSurface {
 id: root
 objectName: "fixtureSurface"
 property Item anchorItem
 property QtObject bar
 property var owner
 property bool open: false
 property Item focusTarget
 property int contentWidth: 380
 property int contentHeight: 500
 default property alias contentItem: holder.children
 width: contentWidth
 height: contentHeight
 visible: open
 padding: Style.spacing.popupPadding
 borderSpec: Border.surfaceSpec("popups","border",Color.popups.border,Math.max(1,Style.space(2)))
 color: Color.popups.background
 radius: Style.cornerRadius
 // Geometry uses the installed host's exact fitting/origin bindings.
 property var screen: ({width:640,height:860})
 property var anchorWindow: ({width:640,height:24})
 property string barPos: "top"
 property int margin: 8
 property int gap: 8
 property bool centerOnBar: false
 property point anchorScreenPos: Qt.point(320,0)
 property real anchorW: 24
 property real anchorH: 24
 __HOST_GEOMETRY__
 x: cardOrigin.x
 y: cardOrigin.y
 onOpenChanged: if(open && focusTarget) Qt.callLater(function(){focusTarget.forceActiveFocus()})
 Item {
  id: holder
  anchors.fill: parent
  anchors.leftMargin: root.contentLeftInset
  anchors.rightMargin: root.contentRightInset
  anchors.topMargin: root.contentTopInset
  anchors.bottomMargin: root.contentBottomInset
 }
}
