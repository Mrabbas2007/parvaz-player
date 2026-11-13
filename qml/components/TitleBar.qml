import QtQuick
import QtQuick.Controls
import ".."

Item {
    id: root

    property string mediaTitle: ""
    property bool alwaysOnTop: false
    property bool isFullscreen: false

    signal pinToggled()
    signal minimizeClicked()
    signal maximizeClicked()
    signal closeClicked()

    height: Theme.titleBarHeight

    // Subtle dark gradient from top to bottom
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#EE0D0E12" }
            GradientStop { position: 0.8; color: "#B30D0E12" }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: windowActions.left
        anchors.rightMargin: 16
        spacing: 10

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 6
            height: 6
            radius: 3
            color: Theme.accent
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, parent.width - 20)
            text: root.mediaTitle.length > 0 ? root.mediaTitle : "Parvaz Player"
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontNormal
            font.bold: true
            elide: Text.ElideRight
        }
    }

    Row {
        id: windowActions
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4

        IconButton {
            iconName: "pin"
            tooltipText: root.alwaysOnTop ? "Disable Always on top (Ctrl+T)" : "Always on top (Ctrl+T)"
            active: root.alwaysOnTop
            buttonSize: 30
            iconSize: 14
            onClicked: root.pinToggled()
        }

        IconButton {
            iconName: "minimize"
            tooltipText: "Minimize"
            buttonSize: 30
            iconSize: 13
            onClicked: root.minimizeClicked()
        }

        IconButton {
            iconName: "maximize"
            tooltipText: root.isFullscreen ? "Restore window" : "Maximize"
            buttonSize: 30
            iconSize: 13
            onClicked: root.maximizeClicked()
        }

        IconButton {
            iconName: "close"
            tooltipText: "Close (Ctrl+Q)"
            buttonSize: 30
            iconSize: 13
            onClicked: root.closeClicked()
        }
    }
}
