import QtQuick
import Parvaz 1.0
import ".."

Item {
    id: root

    property bool isBuffering: Mpv.buffering
    property bool showSpinner: false

    // Debounce timer: ignores sub-150ms micro-loading (instant local file opening)
    // to prevent unwanted visual flicker, while instantly reassuring the user on
    // network streams, 4K seeking, or heavy decodes.
    Timer {
        id: debounceTimer
        interval: 150
        repeat: false
        onTriggered: {
            if (root.isBuffering)
                root.showSpinner = true
        }
    }

    onIsBufferingChanged: {
        if (isBuffering) {
            debounceTimer.restart()
        } else {
            debounceTimer.stop()
            showSpinner = false
        }
    }

    // Pass-through clicks to video controls (fullscreen double-click, pause toggle, etc.)
    enabled: false

    Rectangle {
        id: capsule
        anchors.centerIn: parent
        width: Math.max(96, contentColumn.implicitWidth + 36)
        height: Math.max(88, contentColumn.implicitHeight + 28)
        radius: Theme.radiusLarge
        color: Theme.bgGlass
        border.color: Theme.borderGlass
        border.width: 1

        opacity: root.showSpinner ? 1.0 : 0.0
        visible: opacity > 0.0
        scale: root.showSpinner ? 1.0 : 0.88

        Behavior on opacity {
            NumberAnimation {
                duration: root.showSpinner ? Theme.animNormal : Theme.animFast
                easing.type: Easing.OutCubic
            }
        }
        Behavior on scale {
            NumberAnimation {
                duration: Theme.animNormal
                easing.type: Easing.OutBack
            }
        }

        Column {
            id: contentColumn
            anchors.centerIn: parent
            spacing: 10

            // Smooth High-DPI Circular Spinner
            Item {
                id: spinnerBox
                width: 38
                height: 38
                anchors.horizontalCenter: parent.horizontalCenter

                Canvas {
                    id: spinnerCanvas
                    anchors.fill: parent
                    renderTarget: Canvas.FramebufferObject
                    antialiasing: true

                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.reset();

                        var cx = width / 2;
                        var cy = height / 2;
                        var radius = (width - 6) / 2;

                        // Background subtle ring track
                        ctx.beginPath();
                        ctx.arc(cx, cy, radius, 0, 2 * Math.PI);
                        ctx.lineWidth = 3.0;
                        ctx.strokeStyle = "rgba(255, 255, 255, 0.12)";
                        ctx.stroke();

                        // Active spinning arc
                        ctx.beginPath();
                        ctx.arc(cx, cy, radius, -Math.PI / 2, 0.65 * Math.PI);
                        ctx.lineWidth = 3.0;
                        ctx.lineCap = "round";
                        ctx.strokeStyle = "#FFFFFF";
                        ctx.stroke();
                    }
                }

                RotationAnimation on rotation {
                    from: 0
                    to: 360
                    duration: 900
                    loops: Animation.Infinite
                    running: capsule.visible
                }
            }

            // Status Text (Buffering %, Loading…, or Buffering…)
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                color: Theme.textSecondary
                font.family: Theme.fontDigits
                font.pixelSize: Theme.fontSmall
                font.bold: true

                text: {
                    if (Mpv.bufferPercent > 0 && Mpv.bufferPercent < 100)
                        return "Buffering " + Mpv.bufferPercent + "%"
                    if (Mpv.loading)
                        return "Loading…"
                    return "Buffering…"
                }
            }
        }
    }
}
