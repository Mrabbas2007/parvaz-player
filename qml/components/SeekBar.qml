import QtQuick
import QtQuick.Controls
import Parvaz 1.0
import ".."

Item {
    id: root

    property real position: 0
    property real duration: 0
    property bool isDragging: false
    property real dragPosition: 0

    signal seekRequested(real targetSeconds)
    signal seekingStarted()
    signal seekingFinished()

    implicitHeight: 18

    readonly property real effectiveDuration: Math.max(0.001, duration)
    readonly property real currentPos: isDragging ? dragPosition : position
    readonly property real progressRatio: Math.max(0.0, Math.min(1.0, currentPos / effectiveDuration))

    // Track area
    Item {
        id: trackArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: mouseArea.containsMouse || root.isDragging ? 4.5 : 2.5

        Behavior on height { NumberAnimation { duration: Theme.animFast } }

        // Background rail
        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: Theme.timelineRail
        }

        // Played progress fill
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width * root.progressRatio
            radius: height / 2
            color: Theme.timelinePlayed
        }

        // Scrubber Thumb Handle (only visible on hover or dragging)
        Rectangle {
            id: thumb
            x: (parent.width * root.progressRatio) - (width / 2)
            anchors.verticalCenter: parent.verticalCenter
            width: 10
            height: 10
            radius: 5
            color: "#FFFFFF"
            opacity: mouseArea.containsMouse || root.isDragging ? 1.0 : 0.0

            Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
        }
    }

    readonly property bool hasVideo: Mpv.hasVideo
    property int activeThumbSec: 0

    Timer {
        id: thumbDebounceTimer
        interval: 80
        repeat: false
        onTriggered: {
            root.activeThumbSec = previewTooltip.hoverSec
        }
    }

    // Hover time preview tooltip with thumbnail
    Rectangle {
        id: previewTooltip
        visible: mouseArea.containsMouse && root.duration > 0
        y: trackArea.y - height - 8

        readonly property real hoverRatio: Math.max(0.0, Math.min(1.0, mouseArea.mouseX / Math.max(1, root.width)))
        readonly property real hoverSeconds: hoverRatio * root.effectiveDuration
        readonly property int hoverSec: Math.max(0, Math.round(hoverSeconds))

        onHoverSecChanged: {
            if (root.activeThumbSec === 0 && hoverSec > 0)
                root.activeThumbSec = hoverSec;
            else
                thumbDebounceTimer.restart();
        }

        width: root.hasVideo ? 164 : (previewText.implicitWidth + 14)
        height: root.hasVideo ? 116 : (previewText.implicitHeight + 6)
        radius: Theme.radiusSmall
        color: "#0F1015"
        border.color: "#282C3A"
        border.width: 1

        x: Math.max(8, Math.min(root.width - width - 8, mouseArea.mouseX - (width / 2)))

        Column {
            anchors.centerIn: parent
            spacing: 3

            // Thumbnail container
            Rectangle {
                visible: root.hasVideo
                width: 158
                height: 88
                radius: 2
                color: "#08090C"
                clip: true

                Image {
                    id: thumbImg
                    anchors.fill: parent
                    asynchronous: true
                    cache: true
                    fillMode: Image.PreserveAspectFit
                    source: (root.hasVideo && Mpv.currentFile.length > 0 && previewTooltip.visible)
                                ? ("image://thumbnail/" + encodeURIComponent(Mpv.currentFile) + "?t=" + root.activeThumbSec)
                                : ""

                    opacity: status === Image.Ready ? 1.0 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 100 } }
                }
            }

            // Timestamp readout
            Text {
                id: previewText
                anchors.horizontalCenter: parent.horizontalCenter
                text: Mpv.formatTime(previewTooltip.hoverSeconds)
                color: Theme.textPrimary
                font.family: Theme.fontDigits
                font.pixelSize: Theme.fontSmall
                font.bold: true
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton

        function updateSeek(mouse) {
            var ratio = Math.max(0.0, Math.min(1.0, mouse.x / Math.max(1, root.width)))
            var target = ratio * root.effectiveDuration
            root.dragPosition = target
            root.seekRequested(target)
        }

        onPressed: (mouse) => {
            root.isDragging = true
            root.seekingStarted()
            updateSeek(mouse)
        }

        onPositionChanged: (mouse) => {
            if (root.isDragging)
                updateSeek(mouse)
        }

        onReleased: {
            root.isDragging = false
            root.seekingFinished()
        }
    }
}
