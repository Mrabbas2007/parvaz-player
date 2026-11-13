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

    // Hover time preview tooltip
    Rectangle {
        id: previewTooltip
        visible: mouseArea.containsMouse && root.duration > 0
        y: trackArea.y - height - 6
        width: previewText.implicitWidth + 12
        height: previewText.implicitHeight + 6
        radius: Theme.radiusSmall
        color: Theme.bgTooltip
        border.color: Theme.borderTooltip
        border.width: 1

        readonly property real hoverRatio: Math.max(0.0, Math.min(1.0, mouseArea.mouseX / Math.max(1, root.width)))
        readonly property real hoverSeconds: hoverRatio * root.effectiveDuration

        x: Math.max(0, Math.min(root.width - width, mouseArea.mouseX - (width / 2)))

        Text {
            id: previewText
            anchors.centerIn: parent
            text: Mpv.formatTime(previewTooltip.hoverSeconds)
            color: Theme.textPrimary
            font.family: Theme.fontDigits
            font.pixelSize: Theme.fontSmall
            font.bold: true
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
