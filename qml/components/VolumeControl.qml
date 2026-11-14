import QtQuick
import QtQuick.Controls
import ".."

Row {
    id: root

    property int volume: 100
    property int maxVolume: 150
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
        width: 82
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

        // 100% normal ceiling notch
        Rectangle {
            x: parent.width * (100.0 / root.maxVolume) - 0.5
            anchors.verticalCenter: parent.verticalCenter
            width: 1
            height: 5
            color: "#60FFFFFF"
            z: 1
        }

        // Normal Fill (up to 100%)
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 8.5
            anchors.bottomMargin: 8.5
            width: parent.width * (root.muted ? 0.0 : Math.min(100.0 / root.maxVolume, root.volume / root.maxVolume))
            radius: 1.5
            color: root.muted ? Theme.textMuted : "#FFFFFF"
        }

        // Boost Fill (100% to 150%)
        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: parent.width * (100.0 / root.maxVolume)
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 8.5
            anchors.bottomMargin: 8.5
            width: root.muted || root.volume <= 100 ? 0 : (parent.width * ((root.volume - 100.0) / root.maxVolume))
            radius: 1.5
            color: "#FFAA33"
        }

        // Handle (shown on hover)
        Rectangle {
            x: (parent.width * (root.muted ? 0.0 : (root.volume / root.maxVolume))) - (width / 2)
            anchors.verticalCenter: parent.verticalCenter
            width: 8
            height: 8
            radius: 4
            color: root.volume > 100 ? "#FFAA33" : "#FFFFFF"
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
                var v = Math.round(r * root.maxVolume)
                root.volumeChangeRequested(v)
            }

            onPressed: (mouse) => updateVol(mouse)
            onPositionChanged: (mouse) => {
                if (pressed) updateVol(mouse)
            }
            onWheel: (wheel) => {
                if (wheel.angleDelta.y > 0)
                    root.volumeChangeRequested(Math.min(root.maxVolume, root.volume + 5))
                else if (wheel.angleDelta.y < 0)
                    root.volumeChangeRequested(Math.max(0, root.volume - 5))
            }
        }

        ToolTip {
            visible: volMouse.containsMouse
            text: root.muted ? "Muted" : (root.volume + "%" + (root.volume > 100 ? " (Boost)" : ""))
            delay: 400
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall

            contentItem: Text {
                text: root.muted ? "Muted" : (root.volume + "%" + (root.volume > 100 ? " (Boost)" : ""))
                color: root.volume > 100 ? "#FFAA33" : Theme.textPrimary
                font.family: Theme.fontDigits
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
