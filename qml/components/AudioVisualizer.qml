import QtQuick
import Parvaz 1.0
import ".."

Item {
    id: root

    property int barCount: 24
    property real barWidth: 3.5
    property real barSpacing: 3.0
    property real maxBarHeight: 30
    property real minBarHeight: 3.0
    property bool active: !Mpv.paused && Mpv.duration > 0 && !Mpv.muted

    // Color theme presets: 0 = Electric Sky, 1 = Neon Amber, 2 = Emerald Studio, 3 = Violet Pulse
    property int colorPreset: 0

    readonly property var presetGradients: [
        { top: "#38BDF8", bottom: "#2563EB", glow: "#38BDF840" }, // Electric Sky
        { top: "#F59E0B", bottom: "#D97706", glow: "#F59E0B40" }, // Neon Amber
        { top: "#10B981", bottom: "#059669", glow: "#10B98140" }, // Emerald Studio
        { top: "#A855F7", bottom: "#7C3AED", glow: "#A855F740" }  // Violet Pulse
    ]

    width: barCount * barWidth + (barCount - 1) * barSpacing
    height: maxBarHeight + 8

    // Interactive tooltip and preset cycle on click
    MouseArea {
        id: clickArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.colorPreset = (root.colorPreset + 1) % root.presetGradients.length
        }
    }

    // Main Equalizer Bars (100% driven by real audio stream telemetrics from libmpv)
    Row {
        id: barsRow
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        spacing: root.barSpacing

        Repeater {
            model: root.barCount

            Item {
                width: root.barWidth
                height: root.maxBarHeight

                readonly property real bandValue: (root.active && Mpv.audioBands && Mpv.audioBands.length > index)
                                                   ? Mpv.audioBands[index]
                                                   : 0.0

                // The illuminated equalizer bar
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: root.barWidth
                    height: root.minBarHeight + parent.bandValue * (root.maxBarHeight - root.minBarHeight)
                    radius: root.barWidth / 2

                    gradient: Gradient {
                        GradientStop {
                            position: 0.0
                            color: root.presetGradients[root.colorPreset].top
                        }
                        GradientStop {
                            position: 1.0
                            color: root.presetGradients[root.colorPreset].bottom
                        }
                    }

                    Behavior on height {
                        NumberAnimation {
                            duration: 45
                            easing.type: Easing.OutQuad
                        }
                    }
                }
            }
        }
    }

    // Subtle Reflection Mirror beneath baseline
    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: barsRow.bottom
        spacing: root.barSpacing
        opacity: 0.18

        Repeater {
            model: root.barCount

            Rectangle {
                width: root.barWidth
                readonly property real bandValue: (root.active && Mpv.audioBands && Mpv.audioBands.length > index)
                                                   ? Mpv.audioBands[index]
                                                   : 0.0
                height: Math.min(8, root.minBarHeight + bandValue * 6.0)
                radius: root.barWidth / 2
                color: root.presetGradients[root.colorPreset].top

                Behavior on height {
                    NumberAnimation {
                        duration: 45
                        easing.type: Easing.OutQuad
                    }
                }
            }
        }
    }
}
