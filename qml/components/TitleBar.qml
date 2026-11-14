import QtQuick
import QtQuick.Controls
import ".."

Item {
    id: root

    property string mediaTitle: ""
    property bool alwaysOnTop: false
    property bool isFullscreen: false
    property bool isMaximized: false
    property bool pointerOnBar: barHoverArea.containsMouse

    signal pinToggled()
    signal fullscreenExitClicked()
    signal minimizeClicked()
    signal maximizeToggled()
    signal closeClicked()
    signal startWindowMove()

    height: root.isFullscreen ? 56 : Theme.titleBarHeight
    z: 25

    // Mouse area for hover tracking (prevents auto-hide while hovering title bar)
    // and for window dragging / double-click maximize
    MouseArea {
        id: barHoverArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        onPressed: (mouse) => {
            if (!root.isFullscreen)
                root.startWindowMove()
        }
        onDoubleClicked: (mouse) => {
            if (!root.isFullscreen)
                root.maximizeToggled()
        }
    }

    // 1. Cinematic soft vignette gradient in Fullscreen
    Rectangle {
        anchors.fill: parent
        visible: root.isFullscreen
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#B3000000" }
            GradientStop { position: 0.7; color: "#40000000" }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // 2. Studio Dark Glass background in Windowed / Maximized
    Rectangle {
        anchors.fill: parent
        visible: !root.isFullscreen
        color: Theme.bgGlass
        border.color: "transparent"

        // Bottom hairline separator
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.borderGlassSubtle
        }
    }

    // Left controls: App badge, Media title, Pin button
    Row {
        id: leftControls
        anchors.left: parent.left
        anchors.leftMargin: root.isFullscreen ? 20 : 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8
        z: 2

        // Sleek Parvaz App Emblem
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 20
            height: 20
            radius: 5
            color: "#1FFFFFFF"
            border.color: Theme.borderGlass
            border.width: 1

            Image {
                anchors.centerIn: parent
                width: 10
                height: 10
                source: "../icons/play.svg"
                sourceSize: Qt.size(10, 10)
            }
        }

        // Title text
        Text {
            id: titleText
            anchors.verticalCenter: parent.verticalCenter
            text: root.mediaTitle.length > 0 ? root.mediaTitle : "Parvaz Player"
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontNormal
            font.bold: true
            elide: Text.ElideRight
            width: Math.min(implicitWidth, root.width - (root.isFullscreen ? 130 : 220))
        }

        // Pin Button (Always on Top)
        IconButton {
            id: pinBtn
            anchors.verticalCenter: parent.verticalCenter
            iconName: "pin"
            tooltipText: root.alwaysOnTop ? "Always on top: Active (Ctrl+T)" : "Always on top (Ctrl+T)"
            active: root.alwaysOnTop
            buttonSize: 26
            iconSize: 13
            onClicked: root.pinToggled()
        }
    }

    // Right controls: Window management buttons
    Row {
        id: rightControls
        anchors.right: parent.right
        anchors.rightMargin: root.isFullscreen ? 16 : 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4
        z: 2

        // In Fullscreen: Exit Fullscreen button
        IconButton {
            visible: root.isFullscreen
            iconName: "minimize-2"
            tooltipText: "Exit fullscreen (Esc / F)"
            buttonSize: 28
            iconSize: 15
            onClicked: root.fullscreenExitClicked()
        }

        // In Windowed / Maximized: Minimize button
        IconButton {
            visible: !root.isFullscreen
            iconName: "minus"
            tooltipText: "Minimize"
            buttonSize: 28
            iconSize: 14
            onClicked: root.minimizeClicked()
        }

        // In Windowed / Maximized: Maximize / Restore button
        IconButton {
            visible: !root.isFullscreen
            iconName: root.isMaximized ? "restore" : "square"
            tooltipText: root.isMaximized ? "Restore" : "Maximize"
            buttonSize: 28
            iconSize: 14
            onClicked: root.maximizeToggled()
        }

        // Close button (both Fullscreen and Windowed)
        IconButton {
            iconName: "x"
            tooltipText: "Close (Ctrl+Q)"
            buttonSize: 28
            iconSize: 14
            hoverColor: "#E11D48"
            pressedColor: "#BE123C"
            onClicked: root.closeClicked()
        }
    }
}
