import QtQuick
import QtQuick.Controls
import Parvaz 1.0
import ".."

Item {
    id: root

    property bool isOpen: false

    function toggle() {
        isOpen = !isOpen;
        if (isOpen)
            refresh();
    }

    function open() {
        isOpen = true;
        refresh();
    }

    function close() {
        isOpen = false;
    }

    visible: opacity > 0.0
    opacity: isOpen ? 1.0 : 0.0
    Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }

    property var stats: ({})

    function refresh() {
        if (!isOpen) return;
        stats = Mpv.technicalStats();
    }

    Timer {
        id: refreshTimer
        interval: 500
        repeat: true
        running: root.isOpen
        onTriggered: root.refresh()
    }

    // Semi-transparent glass card
    Rectangle {
        id: card
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 18
        width: Math.min(440, root.width - 36)
        implicitHeight: contentColumn.implicitHeight + 24
        radius: 6
        color: "#E80B0D13"
        border.color: "#283042"
        border.width: 1

        // Drag handler to reposition if desired
        MouseArea {
            id: dragArea
            anchors.fill: parent
            drag.target: card
            drag.minimumX: 8
            drag.maximumX: Math.max(8, root.width - card.width - 8)
            drag.minimumY: 8
            drag.maximumY: Math.max(8, root.height - card.height - 8)
        }

        Column {
            id: contentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 14
            spacing: 12

            // Header
            Row {
                width: parent.width
                spacing: 8

                Text {
                    text: "SYSTEM & STREAM SPECIFICATION"
                    color: "#7E88A0"
                    font.family: Theme.fontFamily
                    font.pixelSize: 10
                    font.bold: true
                    font.letterSpacing: 1.2
                    anchors.verticalCenter: parent.verticalCenter
                }

                Item {
                    id: spacer
                    width: Math.max(0, parent.width - 240)
                    height: 1
                }

                Rectangle {
                    radius: 3
                    color: "#1A2234"
                    border.color: "#2D3952"
                    border.width: 1
                    implicitWidth: keyHint.implicitWidth + 8
                    implicitHeight: 18
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        id: keyHint
                        anchors.centerIn: parent
                        text: "Tab"
                        color: "#9BA5BC"
                        font.family: Theme.fontDigits
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                IconButton {
                    iconName: "x"
                    buttonSize: 20
                    iconSize: 12
                    anchors.verticalCenter: parent.verticalCenter
                    onClicked: root.close()
                }
            }

            // Divider
            Rectangle {
                width: parent.width
                height: 1
                color: "#1F2636"
            }

            // Section 1: Video
            Column {
                width: parent.width
                spacing: 5

                Row {
                    spacing: 6
                    Rectangle {
                        width: 3
                        height: 12
                        radius: 1
                        color: "#3B82F6"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: "VIDEO DECODER"
                        color: "#D2D6E4"
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                Grid {
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 4
                    width: parent.width

                    // Row: Codec & HW
                    Text { text: "Codec:"; color: "#6A748C"; font.pixelSize: 11; font.family: Theme.fontFamily }
                    Row {
                        spacing: 6
                        Text {
                            text: root.stats.videoCodec || "N/A"
                            color: "#FFFFFF"
                            font.pixelSize: 11
                            font.bold: true
                            font.family: Theme.fontDigits
                        }
                        Rectangle {
                            visible: root.stats.hwdec !== undefined
                            radius: 3
                            color: root.stats.isHwdec ? "#0B3629" : "#242834"
                            border.color: root.stats.isHwdec ? "#10B981" : "#444A5C"
                            border.width: 1
                            implicitWidth: hwdecText.implicitWidth + 8
                            implicitHeight: 16
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                id: hwdecText
                                anchors.centerIn: parent
                                text: root.stats.hwdec || "CPU"
                                color: root.stats.isHwdec ? "#34D399" : "#8E95A8"
                                font.pixelSize: 9
                                font.bold: true
                                font.family: Theme.fontDigits
                            }
                        }
                    }

                    // Row: Resolution & Aspect
                    Text { text: "Resolution:"; color: "#6A748C"; font.pixelSize: 11; font.family: Theme.fontFamily }
                    Text {
                        text: (root.stats.resolution || "N/A") + (root.stats.aspect ? ("  (" + root.stats.aspect + ":1)") : "")
                        color: "#E2E5F0"
                        font.pixelSize: 11
                        font.family: Theme.fontDigits
                    }

                    // Row: FPS & Bitrate
                    Text { text: "Framerate:"; color: "#6A748C"; font.pixelSize: 11; font.family: Theme.fontFamily }
                    Text {
                        text: (root.stats.fps || "N/A") + (root.stats.videoBitrate ? (" • " + root.stats.videoBitrate) : "")
                        color: "#E2E5F0"
                        font.pixelSize: 11
                        font.family: Theme.fontDigits
                    }

                    // Row: Dropped frames
                    Text { text: "Frame Drops:"; color: "#6A748C"; font.pixelSize: 11; font.family: Theme.fontFamily }
                    Text {
                        text: (root.stats.voDrops !== undefined ? (root.stats.voDrops + " (VO) / " + root.stats.decDrops + " (Decoder)") : "0")
                        color: (root.stats.voDrops > 0 || root.stats.decDrops > 0) ? "#F87171" : "#34D399"
                        font.pixelSize: 11
                        font.bold: true
                        font.family: Theme.fontDigits
                    }

                    // Row: Profile Mode
                    Text { text: "Profile:"; color: "#6A748C"; font.pixelSize: 11; font.family: Theme.fontFamily }
                    Row {
                        spacing: 6
                        Text {
                            text: (root.stats.performanceProfile ? root.stats.performanceProfile.toUpperCase() : "AUTO")
                            color: "#E2E5F0"
                            font.pixelSize: 11
                            font.bold: true
                            font.family: Theme.fontDigits
                        }
                        Rectangle {
                            radius: 3
                            color: root.stats.isLowSpecActive ? "#3E2612" : "#1A2234"
                            border.color: root.stats.isLowSpecActive ? "#F59E0B" : "#2D3952"
                            border.width: 1
                            implicitWidth: optBadge.implicitWidth + 8
                            implicitHeight: 16
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                id: optBadge
                                anchors.centerIn: parent
                                text: root.stats.isLowSpecActive ? "ECO OPTIMIZED" : "STANDARD"
                                color: root.stats.isLowSpecActive ? "#FBBF24" : "#9BA5BC"
                                font.pixelSize: 9
                                font.bold: true
                                font.family: Theme.fontDigits
                            }
                        }
                    }
                }
            }

            // Divider
            Rectangle {
                width: parent.width
                height: 1
                color: "#1F2636"
            }

            // Section 2: Audio
            Column {
                width: parent.width
                spacing: 5

                Row {
                    spacing: 6
                    Rectangle {
                        width: 3
                        height: 12
                        radius: 1
                        color: "#10B981"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: "AUDIO & DSP"
                        color: "#D2D6E4"
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                Grid {
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 4
                    width: parent.width

                    Text { text: "Format:"; color: "#6A748C"; font.pixelSize: 11; font.family: Theme.fontFamily }
                    Text {
                        text: (root.stats.audioCodec || "N/A") + " • " + (root.stats.audioChannels || "N/A")
                        color: "#FFFFFF"
                        font.pixelSize: 11
                        font.bold: true
                        font.family: Theme.fontDigits
                    }

                    Text { text: "Sample / Bitrate:"; color: "#6A748C"; font.pixelSize: 11; font.family: Theme.fontFamily }
                    Text {
                        text: (root.stats.sampleRate || "N/A") + (root.stats.audioBitrate ? (" • " + root.stats.audioBitrate) : "")
                        color: "#E2E5F0"
                        font.pixelSize: 11
                        font.family: Theme.fontDigits
                    }

                    Text { text: "Night Normalizer:"; color: "#6A748C"; font.pixelSize: 11; font.family: Theme.fontFamily }
                    Row {
                        spacing: 6
                        Text {
                            text: root.stats.audioNormalizer || "Off"
                            color: Mpv.audioNormalizer ? "#FBBF24" : "#8A94A8"
                            font.pixelSize: 11
                            font.bold: Mpv.audioNormalizer
                            font.family: Theme.fontDigits
                        }
                    }
                }
            }

            // Divider
            Rectangle {
                width: parent.width
                height: 1
                color: "#1F2636"
            }

            // Section 3: Performance & Cache
            Column {
                width: parent.width
                spacing: 5

                Row {
                    spacing: 6
                    Rectangle {
                        width: 3
                        height: 12
                        radius: 1
                        color: "#F59E0B"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: "CACHE & SYNC"
                        color: "#D2D6E4"
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                Grid {
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 4
                    width: parent.width

                    Text { text: "Demuxer Buffer:"; color: "#6A748C"; font.pixelSize: 11; font.family: Theme.fontFamily }
                    Text {
                        text: root.stats.cacheDuration || "0.0s"
                        color: "#60A5FA"
                        font.pixelSize: 11
                        font.family: Theme.fontDigits
                    }

                    Text { text: "A/V Sync diff:"; color: "#6A748C"; font.pixelSize: 11; font.family: Theme.fontFamily }
                    Text {
                        text: root.stats.avsync || "0.000s"
                        color: "#E2E5F0"
                        font.pixelSize: 11
                        font.family: Theme.fontDigits
                    }

                    Text { text: "Playback Speed:"; color: "#6A748C"; font.pixelSize: 11; font.family: Theme.fontFamily }
                    Text {
                        text: root.stats.speed || "1.00x"
                        color: "#E2E5F0"
                        font.pixelSize: 11
                        font.family: Theme.fontDigits
                    }
                }
            }
        }
    }
}
