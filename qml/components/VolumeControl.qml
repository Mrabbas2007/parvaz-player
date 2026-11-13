import QtQuick
import QtQuick.Controls
import ".."

Row {
    id: root

    property int volume: 100
    property bool muted: false

    signal volumeChangeRequested(int newVolume)
    signal muteToggled()

    spacing: 6
    anchors.verticalCenter: parent.verticalCenter

    IconButton {
        anchors.verticalCenter: parent.verticalCenter
        iconName: root.muted || root.volume === 0 ? "volume-x" : (root.volume > 40 ? "volume-2" : "volume-1")
        tooltipText: root.muted ? "Unmute (M)" : "Mute (M)"
        buttonSize: 32
        iconSize: 17
        onClicked: root.muteToggled()
    }

    Item {
        id: sliderArea
        anchors.verticalCenter: parent.verticalCenter
        width: 86
        height: 24

        // Rail
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: 4
            radius: 2
            color: "#33FFFFFF"
        }

        // Fill
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 10
            anchors.bottomMargin: 10
            width: parent.width * (root.muted ? 0.0 : (root.volume / 100.0))
            radius: 2
            color: root.muted ? Theme.textMuted : Theme.accent
        }

        // Handle
        Rectangle {
            x: (parent.width * (root.muted ? 0.0 : (root.volume / 100.0))) - (width / 2)
            anchors.verticalCenter: parent.verticalCenter
            width: volMouse.containsMouse || volMouse.pressed ? 12 : 8
            height: width
            radius: width / 2
            color: "#FFFFFF"
            border.color: Theme.accent
            border.width: 1.5

            Behavior on width { NumberAnimation { duration: Theme.animFast } }
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
