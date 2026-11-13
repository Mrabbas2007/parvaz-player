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

    implicitHeight: 24

    readonly property real effectiveDuration: Math.max(0.001, duration)
    readonly property real currentPos: isDragging ? dragPosition : position
    readonly property real progressRatio: Math.max(0.0, Math.min(1.0, currentPos / effectiveDuration))

    // Track background
    Item {
        id: trackArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: mouseArea.containsMouse || root.isDragging ? 6 : 3.5

        Behavior on height { NumberAnimation { duration: Theme.animFast } }

        // Background rail
        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: "#2EFFFFFF"
        }

        // Progress fill
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width * root.progressRatio
            radius: height / 2

            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Theme.accentGradientStart }
                GradientStop { position: 1.0; color: Theme.accentGradientEnd }
            }
        }

        // Scrubber Thumb Handle
        Rectangle {
            id: thumb
            x: (parent.width * root.progressRatio) - (width / 2)
            anchors.verticalCenter: parent.verticalCenter
            width: mouseArea.containsMouse || root.isDragging ? 13 : 8
            height: width
            radius: width / 2
            color: "#FFFFFF"
            border.color: Theme.accent
            border.width: mouseArea.containsMouse || root.isDragging ? 2.5 : 1.5

            Behavior on width { NumberAnimation { duration: Theme.animFast } }
        }
    }

    // Hover time preview tooltip
    Rectangle {
        id: previewTooltip
        visible: mouseArea.containsMouse && root.duration > 0
        y: trackArea.y - height - 8
        width: previewText.implicitWidth + 14
        height: previewText.implicitHeight + 8
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
            font.family: Theme.fontFamily
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
