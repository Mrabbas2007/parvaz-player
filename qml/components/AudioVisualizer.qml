import QtQuick
import Parvaz 1.0
import ".."

Item {
    id: root

    property int barCount: 24
    property real barWidth: 3.5
    property real barSpacing: 3.0
    property real maxBarHeight: 32
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

    // Internal harmonic state for realistic multi-frequency spectrum
    property real phase: 0.0
    property var barHeights: []

    Component.onCompleted: {
        var initial = []
        for (var i = 0; i < barCount; ++i) {
            initial.push(minBarHeight)
        }
        barHeights = initial
    }

    // 25Hz harmonic calculation with 60fps Scenegraph easing
    Timer {
        id: animTimer
        interval: 40
        repeat: true
        running: root.active && root.visible
        onTriggered: {
            root.phase += 0.22
            var t = root.phase
            var newHeights = []

            // Rhythmic kick simulating beat rhythm
            var beat = Math.pow(Math.max(0, Math.sin(t * 1.8)), 4)
            var snare = Math.pow(Math.max(0, Math.sin(t * 3.6 + 1.2)), 3)

            for (var i = 0; i < root.barCount; ++i) {
                var pos = i / (root.barCount - 1)

                // Sub-bass & Bass envelope (left side 0.0 - 0.3)
                var bassWeight = Math.exp(-Math.pow((pos - 0.15) / 0.2, 2))
                var bassVal = (0.25 + 0.75 * beat) * (0.6 + 0.4 * Math.sin(t * 3.2 + i * 0.4)) * bassWeight

                // Midrange / Vocals envelope (center 0.3 - 0.7)
                var midWeight = Math.exp(-Math.pow((pos - 0.5) / 0.25, 2))
                var midVal = (0.3 + 0.7 * snare) * (0.5 + 0.5 * Math.sin(t * 5.4 + i * 0.6)) * midWeight

                // Treble / Cymbals envelope (right side 0.7 - 1.0)
                var trebleWeight = Math.exp(-Math.pow((pos - 0.85) / 0.2, 2))
                var trebleVal = (0.4 + 0.6 * Math.sin(t * 7.8 + i * 0.8)) * trebleWeight

                // Organic noise modulation
                var noise = 0.15 * Math.sin(t * 9.2 + i * 1.5)

                var raw = bassVal + midVal + trebleVal + noise
                var norm = Math.max(0.08, Math.min(1.0, raw))

                newHeights.push(root.minBarHeight + norm * (root.maxBarHeight - root.minBarHeight))
            }
            root.barHeights = newHeights
        }
    }

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

    // Main Equalizer Bars
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

                readonly property real targetHeight: (root.active && root.barHeights.length > index)
                                                      ? root.barHeights[index]
                                                      : root.minBarHeight

                // The illuminated equalizer bar
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: root.barWidth
                    height: targetHeight
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
                            duration: root.active ? 85 : 300
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
                height: Math.min(8, (root.active && root.barHeights.length > index ? root.barHeights[index] * 0.25 : root.minBarHeight))
                radius: root.barWidth / 2
                color: root.presetGradients[root.colorPreset].top

                Behavior on height {
                    NumberAnimation {
                        duration: root.active ? 85 : 300
                        easing.type: Easing.OutQuad
                    }
                }
            }
        }
    }
}
