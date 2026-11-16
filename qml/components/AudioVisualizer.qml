import QtQuick
import Parvaz 1.0
import ".."

Item {
    id: root

    property int barCount: 32
    property real barWidth: 4.5
    property real barSpacing: 3.5
    property real maxBarHeight: 18
    property real minBarHeight: 2.5
    property bool active: !Mpv.paused && Mpv.duration > 0 && !Mpv.muted

    // Theme-driven dynamic gradient (automatically updates when theme changes)
    readonly property color topColor: {
        if (Theme.currentTheme === "potplayer") return "#FBBF24";
        if (Theme.currentTheme === "cyber") return "#22D3EE";
        if (Theme.currentTheme === "oled") return "#F4F4F5";
        return "#60A5FA"; // midnight studio default
    }

    readonly property color bottomColor: {
        if (Theme.currentTheme === "potplayer") return "#D97706";
        if (Theme.currentTheme === "cyber") return "#0284C7";
        if (Theme.currentTheme === "oled") return "#71717A";
        return "#2563EB"; // midnight studio default
    }

    width: barCount * barWidth + (barCount - 1) * barSpacing
    height: maxBarHeight + 8

    // Main Equalizer Bars (100% driven by real audio stream telemetrics from libmpv)
    Row {
        id: barsRow
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 7
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
                        GradientStop { position: 0.0; color: root.topColor }
                        GradientStop { position: 1.0; color: root.bottomColor }
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
        opacity: 0.16

        Repeater {
            model: root.barCount

            Rectangle {
                width: root.barWidth
                readonly property real bandValue: (root.active && Mpv.audioBands && Mpv.audioBands.length > index)
                                                   ? Mpv.audioBands[index]
                                                   : 0.0
                height: Math.min(5, root.minBarHeight + bandValue * 4.0)
                radius: root.barWidth / 2
                color: root.topColor

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
