import QtQuick
import QtQuick.Controls
import ".."

Item {
    id: root

    property string mediaTitle: ""
    property bool alwaysOnTop: false
    property bool isFullscreen: false

    signal pinToggled()
    signal fullscreenExitClicked()

    height: 56

    // Soft cinematic top vignette gradient
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#B3000000" }
            GradientStop { position: 0.7; color: "#40000000" }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // Top-left title and pin
    Row {
        anchors.left: parent.left
        anchors.leftMargin: 20
        anchors.top: parent.top
        anchors.topMargin: 12
        spacing: 10

        Text {
            id: titleText
            anchors.verticalCenter: parent.verticalCenter
            text: root.mediaTitle.length > 0 ? root.mediaTitle : "Parvaz"
            color: "#FFFFFF"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontMedium
            font.bold: true
            elide: Text.ElideRight
            width: Math.min(implicitWidth, root.width - (root.isFullscreen ? 120 : 80))
        }

        IconButton {
            id: pinBtn
            anchors.verticalCenter: parent.verticalCenter
            iconName: "pin"
            tooltipText: root.alwaysOnTop ? "Always on top: Active (Ctrl+T)" : "Always on top (Ctrl+T)"
            active: root.alwaysOnTop
            buttonSize: 26
            iconSize: 14
            onClicked: root.pinToggled()
        }
    }

    // Exit fullscreen button (only shown when in Fullscreen mode)
    IconButton {
        visible: root.isFullscreen
        anchors.right: parent.right
        anchors.rightMargin: 18
        anchors.top: parent.top
        anchors.topMargin: 10
        iconName: "minimize-2"
        tooltipText: "Exit fullscreen (Esc / F)"
        buttonSize: 32
        iconSize: 17
        onClicked: root.fullscreenExitClicked()
    }
}
