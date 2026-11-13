import QtQuick
import Parvaz 1.0
import ".."

Item {
    id: root

    anchors.fill: parent

    function show(text) {
        osdText.text = text
        osdCapsule.opacity = 1.0
        hideTimer.restart()
    }

    Rectangle {
        id: osdCapsule
        anchors.top: parent.top
        anchors.topMargin: 56 // below the top title bar
        anchors.right: parent.right
        anchors.rightMargin: 24
        width: osdText.implicitWidth + 28
        height: osdText.implicitHeight + 14
        radius: Theme.radiusMedium
        color: Theme.bgGlass
        border.color: Theme.borderTooltip
        border.width: 1
        opacity: 0.0
        visible: opacity > 0.0

        Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }

        Row {
            anchors.centerIn: parent
            spacing: 8

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 5
                height: 5
                radius: 2.5
                color: Theme.accent
            }

            Text {
                id: osdText
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontMedium
                font.bold: true
            }
        }

        Timer {
            id: hideTimer
            interval: 1600
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
