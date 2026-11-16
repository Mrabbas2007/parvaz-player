import QtQuick
import Parvaz 1.0
import ".."

Item {
    id: root

    property int topInset: 0

    anchors.fill: parent

    function show(text) {
        osdText.text = text
        osdCapsule.opacity = 1.0
        hideTimer.restart()
    }

    Rectangle {
        id: osdCapsule
        anchors.top: parent.top
        anchors.topMargin: root.topInset + 16
        anchors.left: parent.left
        anchors.leftMargin: 24
        width: osdText.implicitWidth + 24
        height: osdText.implicitHeight + 12
        radius: Theme.radiusMedium
        color: Theme.bgTooltip
        border.color: Theme.borderTooltip
        border.width: 1
        opacity: 0.0
        visible: opacity > 0.0

        Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
        Behavior on anchors.topMargin { NumberAnimation { duration: Theme.animNormal } }

        Text {
            id: osdText
            anchors.centerIn: parent
            color: "#FFFFFF"
            font.family: Theme.fontDigits
            font.pixelSize: Theme.fontNormal
            font.bold: true
        }

        Timer {
            id: hideTimer
            interval: 1500
            repeat: false
            onTriggered: osdCapsule.opacity = 0.0
        }
    }

    Connections {
        target: Mpv
        function onOsdRequested(text) {
            root.show(text)
        }
    }
}
