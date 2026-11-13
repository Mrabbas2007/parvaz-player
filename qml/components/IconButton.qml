import QtQuick
import QtQuick.Controls
import ".."

Item {
    id: root

    property string iconName: ""
    property string iconText: ""
    property color iconColor: active ? Theme.accent : (mouseArea.containsMouse ? Theme.textPrimary : Theme.textSecondary)
    property real iconSize: 16
    property real buttonSize: 34
    property string tooltipText: ""
    property bool active: false
    property bool isRound: true

    signal clicked()

    width: buttonSize
    height: buttonSize

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: root.isRound ? width / 2 : Theme.radiusMedium
        color: root.active
               ? Theme.bgGlassLight
               : (mouseArea.pressed
                  ? Theme.bgGlassPressed
                  : (mouseArea.containsMouse ? Theme.bgGlassHover : "transparent"))
        border.color: root.active ? Theme.borderGlassActive : (mouseArea.containsMouse ? Theme.borderGlassSubtle : "transparent")
        border.width: 1

        Behavior on color { ColorAnimation { duration: Theme.animFast } }
        Behavior on border.color { ColorAnimation { duration: Theme.animFast } }
    }

    scale: mouseArea.pressed ? 0.92 : 1.0
    Behavior on scale { NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutQuad } }

    Canvas {
        id: canvas
        anchors.centerIn: parent
        width: root.iconSize
        height: root.iconSize
        visible: root.iconText.length === 0

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = root.iconColor
            ctx.strokeStyle = root.iconColor
            ctx.lineWidth = 1.6
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            var w = width
            var h = height

            if (root.iconName === "play") {
                ctx.beginPath()
                ctx.moveTo(w * 0.22, h * 0.12)
                ctx.lineTo(w * 0.88, h * 0.50)
                ctx.lineTo(w * 0.22, h * 0.88)
                ctx.closePath()
                ctx.fill()
            } else if (root.iconName === "pause") {
                var barW = w * 0.24
                ctx.fillRect(w * 0.18, h * 0.15, barW, h * 0.70)
                ctx.fillRect(w * 0.58, h * 0.15, barW, h * 0.70)
            } else if (root.iconName === "stop") {
                ctx.fillRect(w * 0.18, h * 0.18, w * 0.64, h * 0.64)
            } else if (root.iconName === "step-forward") {
                ctx.beginPath()
                ctx.moveTo(w * 0.15, h * 0.20)
                ctx.lineTo(w * 0.55, h * 0.50)
                ctx.lineTo(w * 0.15, h * 0.80)
                ctx.closePath()
                ctx.fill()
                ctx.beginPath()
                ctx.moveTo(w * 0.50, h * 0.20)
                ctx.lineTo(w * 0.85, h * 0.50)
                ctx.lineTo(w * 0.50, h * 0.80)
                ctx.closePath()
                ctx.fill()
            } else if (root.iconName === "step-back") {
                ctx.beginPath()
                ctx.moveTo(w * 0.50, h * 0.20)
                ctx.lineTo(w * 0.15, h * 0.50)
                ctx.lineTo(w * 0.50, h * 0.80)
                ctx.closePath()
                ctx.fill()
                ctx.beginPath()
                ctx.moveTo(w * 0.85, h * 0.20)
                ctx.lineTo(w * 0.50, h * 0.50)
                ctx.lineTo(w * 0.85, h * 0.80)
                ctx.closePath()
                ctx.fill()
            } else if (root.iconName === "volume-high") {
                // Speaker cone
                ctx.beginPath()
                ctx.moveTo(w * 0.12, h * 0.35)
                ctx.lineTo(w * 0.32, h * 0.35)
                ctx.lineTo(w * 0.55, h * 0.15)
                ctx.lineTo(w * 0.55, h * 0.85)
                ctx.lineTo(w * 0.32, h * 0.65)
                ctx.lineTo(w * 0.12, h * 0.65)
                ctx.closePath()
                ctx.fill()
                // Sound arc 1
                ctx.beginPath()
                ctx.arc(w * 0.55, h * 0.50, w * 0.22, -Math.PI * 0.30, Math.PI * 0.30)
                ctx.stroke()
                // Sound arc 2
                ctx.beginPath()
                ctx.arc(w * 0.55, h * 0.50, w * 0.36, -Math.PI * 0.32, Math.PI * 0.32)
                ctx.stroke()
            } else if (root.iconName === "volume-muted") {
                // Speaker cone
                ctx.beginPath()
                ctx.moveTo(w * 0.12, h * 0.35)
                ctx.lineTo(w * 0.32, h * 0.35)
                ctx.lineTo(w * 0.55, h * 0.15)
                ctx.lineTo(w * 0.55, h * 0.85)
                ctx.lineTo(w * 0.32, h * 0.65)
                ctx.lineTo(w * 0.12, h * 0.65)
                ctx.closePath()
                ctx.fill()
                // Mute cross
                ctx.beginPath()
                ctx.moveTo(w * 0.68, h * 0.36)
                ctx.lineTo(w * 0.90, h * 0.64)
                ctx.moveTo(w * 0.90, h * 0.36)
                ctx.lineTo(w * 0.68, h * 0.64)
                ctx.stroke()
            } else if (root.iconName === "fullscreen") {
                var s = w * 0.28
                // Top-left
                ctx.beginPath(); ctx.moveTo(0, s); ctx.lineTo(0, 0); ctx.lineTo(s, 0); ctx.stroke()
                // Top-right
                ctx.beginPath(); ctx.moveTo(w - s, 0); ctx.lineTo(w, 0); ctx.lineTo(w, s); ctx.stroke()
                // Bottom-left
                ctx.beginPath(); ctx.moveTo(0, h - s); ctx.lineTo(0, h); ctx.lineTo(s, h); ctx.stroke()
                // Bottom-right
                ctx.beginPath(); ctx.moveTo(w - s, h); ctx.lineTo(w, h); ctx.lineTo(w, h - s); ctx.stroke()
            } else if (root.iconName === "fullscreen-exit") {
                var s2 = w * 0.28
                // Top-left into center
                ctx.beginPath(); ctx.moveTo(0, 0); ctx.lineTo(s2, 0); ctx.lineTo(s2, s2); ctx.stroke()
                // Top-right into center
                ctx.beginPath(); ctx.moveTo(w, 0); ctx.lineTo(w - s2, 0); ctx.lineTo(w - s2, s2); ctx.stroke()
                // Bottom-left into center
                ctx.beginPath(); ctx.moveTo(0, h); ctx.lineTo(s2, h); ctx.lineTo(s2, h - s2); ctx.stroke()
                // Bottom-right into center
                ctx.beginPath(); ctx.moveTo(w, h); ctx.lineTo(w - s2, h); ctx.lineTo(w - s2, h - s2); ctx.stroke()
            } else if (root.iconName === "pin") {
                // Pin head / needle
                ctx.beginPath()
                ctx.moveTo(w * 0.30, h * 0.15)
                ctx.lineTo(w * 0.70, h * 0.15)
                ctx.lineTo(w * 0.58, h * 0.50)
                ctx.lineTo(w * 0.75, h * 0.60)
                ctx.lineTo(w * 0.25, h * 0.60)
                ctx.lineTo(w * 0.42, h * 0.50)
                ctx.closePath()
                ctx.fill()
                ctx.beginPath()
                ctx.moveTo(w * 0.50, h * 0.60)
                ctx.lineTo(w * 0.50, h * 0.90)
                ctx.stroke()
            } else if (root.iconName === "minimize") {
                ctx.beginPath()
                ctx.moveTo(w * 0.20, h * 0.75)
                ctx.lineTo(w * 0.80, h * 0.75)
                ctx.stroke()
            } else if (root.iconName === "maximize") {
                ctx.strokeRect(w * 0.20, h * 0.20, w * 0.60, h * 0.60)
            } else if (root.iconName === "close") {
                ctx.beginPath()
                ctx.moveTo(w * 0.25, h * 0.25)
                ctx.lineTo(w * 0.75, h * 0.75)
                ctx.moveTo(w * 0.75, h * 0.25)
                ctx.lineTo(w * 0.25, h * 0.75)
                ctx.stroke()
            } else if (root.iconName === "subtitles") {
                ctx.strokeRect(w * 0.10, h * 0.20, w * 0.80, h * 0.60)
                ctx.fillRect(w * 0.25, h * 0.60, w * 0.22, h * 0.10)
                ctx.fillRect(w * 0.53, h * 0.60, w * 0.22, h * 0.10)
            } else if (root.iconName === "audio") {
                ctx.beginPath()
                ctx.arc(w * 0.30, h * 0.70, w * 0.16, 0, Math.PI * 2)
                ctx.fill()
                ctx.beginPath()
                ctx.moveTo(w * 0.44, h * 0.70)
                ctx.lineTo(w * 0.44, h * 0.20)
                ctx.lineTo(w * 0.80, h * 0.30)
                ctx.lineTo(w * 0.80, h * 0.70)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(w * 0.66, h * 0.70, w * 0.16, 0, Math.PI * 2)
                ctx.fill()
            } else if (root.iconName === "open") {
                ctx.beginPath()
                ctx.moveTo(w * 0.12, h * 0.25)
                ctx.lineTo(w * 0.45, h * 0.25)
                ctx.lineTo(w * 0.55, h * 0.38)
                ctx.lineTo(w * 0.88, h * 0.38)
                ctx.lineTo(w * 0.88, h * 0.78)
                ctx.lineTo(w * 0.12, h * 0.78)
                ctx.closePath()
                ctx.stroke()
            }
        }

        Connections {
            target: root
            function onIconColorChanged() { canvas.requestPaint() }
            function onIconNameChanged() { canvas.requestPaint() }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: root.iconText.length > 0
        text: root.iconText
        color: root.iconColor
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontNormal
        font.bold: true
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
        delay: 500
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
