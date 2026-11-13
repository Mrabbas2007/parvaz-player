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

    height: 48

    // Top-left floating title pill
    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.top: parent.top
        anchors.topMargin: 12
        height: 36
        width: Math.min(titleText.implicitWidth + (pinBtn.visible ? 44 : 20) + 24, root.width - (root.isFullscreen ? 90 : 32))
        radius: Theme.radiusLarge
        color: Theme.bgGlass
        border.color: Theme.borderGlass
        border.width: 1

        Row {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 8
            spacing: 8

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 6
                height: 6
                radius: 3
                color: Theme.accent
            }

            Text {
                id: titleText
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 24 - (pinBtn.visible ? pinBtn.width + 8 : 0)
                text: root.mediaTitle.length > 0 ? root.mediaTitle : "Parvaz Player"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontNormal
                font.bold: true
                elide: Text.ElideRight
            }

            IconButton {
                id: pinBtn
                anchors.verticalCenter: parent.verticalCenter
                iconName: "pin"
                tooltipText: root.alwaysOnTop ? "Disable Always on top (Ctrl+T)" : "Always on top (Ctrl+T)"
                active: root.alwaysOnTop
                buttonSize: 28
                iconSize: 15
                onClicked: root.pinToggled()
            }
        }
    }

    // Top-right exit fullscreen button (only shown when in Fullscreen mode)
    IconButton {
        visible: root.isFullscreen
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.top: parent.top
        anchors.topMargin: 12
        iconName: "minimize-2"
        tooltipText: "Exit fullscreen (Esc / F)"
        buttonSize: 36
        iconSize: 18
        onClicked: root.fullscreenExitClicked()
    }
}
