import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import ".."

Item {
    id: root

    property string iconName: ""
    property color iconColor: active ? "#FFFFFF" : (mouseArea.containsMouse ? "#FFFFFF" : Theme.textSecondary)
    property real iconSize: 18
    property real buttonSize: 32
    property string tooltipText: ""
    property bool active: false
    property bool isRound: true
    property color hoverColor: "#14FFFFFF"
    property color pressedColor: "#33FFFFFF"

    signal clicked()

    width: buttonSize
    height: buttonSize

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: root.isRound ? width / 2 : Theme.radiusMedium
        color: root.active
               ? "#26FFFFFF"
               : (mouseArea.pressed
                  ? root.pressedColor
                  : (mouseArea.containsMouse ? root.hoverColor : "transparent"))

        Behavior on color { ColorAnimation { duration: Theme.animFast } }
    }

    scale: mouseArea.pressed ? 0.94 : 1.0
    Behavior on scale { NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutQuad } }

    Image {
        id: iconImg
        anchors.centerIn: parent
        width: root.iconSize
        height: root.iconSize
        source: root.iconName.length > 0 ? "../icons/" + root.iconName + ".svg" : ""
        sourceSize: Qt.size(Math.round(root.iconSize * 3), Math.round(root.iconSize * 3))
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        visible: false
    }

    MultiEffect {
        anchors.fill: iconImg
        source: iconImg
        colorization: 1.0
        colorizationColor: root.iconColor
        opacity: root.active ? 1.0 : (mouseArea.containsMouse ? 1.0 : 0.78)

        Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        onClicked: root.clicked()
    }

    ToolTip {
        visible: root.tooltipText.length > 0 && mouseArea.containsMouse
        delay: 450
        timeout: 2500
        text: root.tooltipText
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSmall

        contentItem: Text {
            text: root.tooltipText
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
