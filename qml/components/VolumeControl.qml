import QtQuick
import QtQuick.Controls
import ".."

Row {
    id: root

    property int volume: 100
    property bool muted: false

    signal volumeChangeRequested(int newVolume)
    signal muteToggled()

    spacing: 4
    anchors.verticalCenter: parent.verticalCenter

    IconButton {
        anchors.verticalCenter: parent.verticalCenter
        iconName: root.muted || root.volume === 0 ? "volume-x" : (root.volume > 40 ? "volume-2" : "volume-1")
        tooltipText: root.muted ? "Unmute (M)" : "Mute (M)"
        buttonSize: 30
        iconSize: 17
        onClicked: root.muteToggled()
    }

    Item {
        id: sliderArea
        anchors.verticalCenter: parent.verticalCenter
        width: 74
        height: 20

        // Rail
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: 3
            radius: 1.5
            color: Theme.timelineRail
        }

        // Fill
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 8.5
            anchors.bottomMargin: 8.5
            width: parent.width * (root.muted ? 0.0 : (root.volume / 100.0))
            radius: 1.5
            color: root.muted ? Theme.textMuted : "#FFFFFF"
        }

        // Handle (shown on hover)
        Rectangle {
            x: (parent.width * (root.muted ? 0.0 : (root.volume / 100.0))) - (width / 2)
            anchors.verticalCenter: parent.verticalCenter
            width: 8
            height: 8
            radius: 4
            color: "#FFFFFF"
            opacity: volMouse.containsMouse || volMouse.pressed ? 1.0 : 0.0

            Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
        }

        MouseArea {
            id: volMouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton

            function updateVol(mouse) {
                var r = Math.max(0.0, Math.min(1.0, mouse.x / sliderArea.width))
                var v = Math.round(r * 100)
                root.volumeChangeRequested(v)
            }

            onPressed: (mouse) => updateVol(mouse)
            onPositionChanged: (mouse) => {
                if (pressed) updateVol(mouse)
            }
        }

        ToolTip {
            visible: volMouse.containsMouse
            text: root.muted ? "Muted" : root.volume + "%"
            delay: 400
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall

            contentItem: Text {
                text: root.muted ? "Muted" : root.volume + "%"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
            }

            background: Rectangle {
                color: Theme.bgTooltip
                border.color: Theme.borderTooltip
                border.width: 1
                radius: Theme.radiusSmall
            }
        }
    }
}
