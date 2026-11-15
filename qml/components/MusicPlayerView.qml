import QtQuick
import QtQuick.Controls
import Parvaz 1.0
import ".."

Item {
    id: root

    readonly property bool isAudio: Mpv.isAudioOnly
    readonly property bool hasCover: Mpv.coverArtUrl.length > 0
    readonly property real cardSize: Math.max(160, Math.min(320, Math.min(parent.width - 60, parent.height - 230)))

    visible: isAudio
    opacity: isAudio ? 1.0 : 0.0
    Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }

    // --- Dynamic Floating & Breathing Animations ---
    property real floatOffset: 0.0
    property real pulseScale: 1.0
    property real wavePhase: 0.0

    // Smooth harmonic floating motion (Sine oscillation)
    SequentialAnimation {
        id: floatAnim
        running: !Mpv.paused && root.isAudio
        loops: Animation.Infinite

        NumberAnimation {
            target: root
            property: "floatOffset"
            to: -6.0
            duration: 2200
            easing.type: Easing.InOutSine
        }
        NumberAnimation {
            target: root
            property: "floatOffset"
            to: 4.0
            duration: 2200
            easing.type: Easing.InOutSine
        }
    }

    Behavior on floatOffset {
        NumberAnimation { duration: 600; easing.type: Easing.OutCubic }
    }

    // Gentle rhythmic pulse
    SequentialAnimation {
        id: pulseAnim
        running: !Mpv.paused && root.isAudio
        loops: Animation.Infinite

        NumberAnimation {
            target: root
            property: "pulseScale"
            to: 1.02
            duration: 1200
            easing.type: Easing.InOutQuad
        }
        NumberAnimation {
            target: root
            property: "pulseScale"
            to: 1.0
            duration: 1200
            easing.type: Easing.InOutQuad
        }
    }

    Behavior on pulseScale {
        NumberAnimation { duration: 450; easing.type: Easing.OutCubic }
    }

    // Visualizer wave phase driver
    NumberAnimation on wavePhase {
        from: 0
        to: 2 * Math.PI
        duration: 2600
        loops: Animation.Infinite
        running: !Mpv.paused && root.isAudio
    }

    // Ambient background atmosphere
    Rectangle {
        anchors.fill: parent
        color: "#08090D"

        // Ambient Cover Glow (Blurred & Darkened)
        Image {
            id: ambientBg
            anchors.fill: parent
            source: root.hasCover ? Mpv.coverArtUrl : ""
            fillMode: Image.PreserveAspectCrop
            opacity: root.hasCover ? 0.16 : 0.0
            asynchronous: true
            visible: opacity > 0.0

            Behavior on opacity { NumberAnimation { duration: 400 } }
        }

        // Gradient overlay for smooth contrast
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#CC08090D" }
                GradientStop { position: 0.5; color: "#B308090D" }
                GradientStop { position: 1.0; color: "#F208090D" }
            }
        }
    }

    // Main Center Content with Floating Offset & Pulse
    Item {
        id: mainContent
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -24 + root.floatOffset
        scale: root.pulseScale
        width: Math.max(root.cardSize, metaColumn.implicitWidth)
        height: root.cardSize + metaColumn.implicitHeight + 20

        // Poster / Album Card Container
        Item {
            id: posterArea
            anchors.horizontalCenter: parent.horizontalCenter
            width: root.cardSize
            height: root.cardSize

            // --- 1. Glowing Ambient Halo (Behind Poster) ---
            Item {
                id: ambientHalo
                anchors.centerIn: parent
                width: parent.width * 1.35
                height: parent.height * 1.35
                z: 0

                // Broad soft atmospheric aura
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width * (root.pulseScale * 0.95)
                    height: parent.height * (root.pulseScale * 0.95)
                    radius: width / 2
                    color: root.hasCover ? "#306366F1" : "#203B82F6"
                    opacity: (!Mpv.paused && root.isAudio) ? 0.55 : 0.18

                    Behavior on opacity { NumberAnimation { duration: 600 } }
                    Behavior on color { ColorAnimation { duration: 500 } }
                }

                // Vibrant chromatic core ring
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width * 0.8
                    height: parent.height * 0.8
                    radius: width / 2
                    color: root.hasCover ? "#25818CF8" : "#1A60A5FA"
                    opacity: (!Mpv.paused && root.isAudio) ? 0.70 : 0.22

                    Behavior on opacity { NumberAnimation { duration: 500 } }
                }
            }

            // --- 2. Live Dynamic Acoustic Spectrum Waves (Hugging Poster Bottom) ---
            Row {
                id: spectrumRow
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: -10
                spacing: 4
                z: 0

                Repeater {
                    model: 26

                    Rectangle {
                        required property int index
                        width: 3
                        radius: 1.5

                        // Multi-harmonic wave equation for natural, living audio bounce
                        height: {
                            if (!Mpv.paused && root.isAudio) {
                                var s1 = Math.sin(root.wavePhase * 2.0 + index * 0.38);
                                var s2 = Math.cos(root.wavePhase * 1.2 + index * 0.22);
                                var amp = Math.abs(s1 * s2);
                                return 6 + amp * 28;
                            }
                            return 4;
                        }

                        color: {
                            var a = (!Mpv.paused && root.isAudio)
                                ? (0.40 + 0.35 * Math.abs(Math.sin(root.wavePhase + index * 0.25)))
                                : 0.18;
                            return Qt.rgba(0.75, 0.82, 1.0, a);
                        }

                        Behavior on height { NumberAnimation { duration: 140; easing.type: Easing.OutQuad } }
                        Behavior on color { ColorAnimation { duration: 250 } }
                    }
                }
            }

            // --- 3. The Cover Art Card (When Cover is Available) ---
            Rectangle {
                id: coverCard
                anchors.fill: parent
                radius: 16
                color: "#12141D"
                border.color: Theme.borderGlass
                border.width: 1
                clip: true
                visible: root.hasCover
                z: 1

                Image {
                    id: coverImg
                    anchors.fill: parent
                    source: root.hasCover ? Mpv.coverArtUrl : ""
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true

                    opacity: status === Image.Ready ? 1.0 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 250 } }
                }

                // Hover overlay with play/pause indicator
                Rectangle {
                    anchors.fill: parent
                    color: "#50000000"
                    opacity: posterMouse.containsMouse ? 1.0 : 0.0
                    Behavior on opacity { NumberAnimation { duration: Theme.animFast } }

                    Image {
                        anchors.centerIn: parent
                        width: 48
                        height: 48
                        source: Mpv.paused ? "../icons/play.svg" : "../icons/pause.svg"
                        sourceSize: Qt.size(48, 48)
                    }
                }
            }

            // --- 4. Fallback: Studio Obsidian Vinyl Disc (When No Cover Exists) ---
            Item {
                id: vinylDisc
                anchors.fill: parent
                visible: !root.hasCover
                z: 1

                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: "#0D0F15"
                    border.color: "#1F2536"
                    border.width: 2

                    // Concentric Micro-Grooves
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 0.86
                        height: width
                        radius: width / 2
                        color: "transparent"
                        border.color: "#161924"
                        border.width: 1.5
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 0.72
                        height: width
                        radius: width / 2
                        color: "transparent"
                        border.color: "#1A1E2B"
                        border.width: 1.5
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 0.58
                        height: width
                        radius: width / 2
                        color: "transparent"
                        border.color: "#1F2433"
                        border.width: 1.5
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 0.44
                        height: width
                        radius: width / 2
                        color: "transparent"
                        border.color: "#161924"
                        border.width: 1.5
                    }

                    // Center Vinyl Label with Studio Accent
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 0.34
                        height: width
                        radius: width / 2
                        color: "#131622"
                        border.color: "#3B82F6"
                        border.width: 1.5

                        Image {
                            anchors.centerIn: parent
                            width: 26
                            height: 26
                            source: "../icons/music.svg"
                            sourceSize: Qt.size(26, 26)
                        }

                        // Center Spindle Hole
                        Rectangle {
                            anchors.centerIn: parent
                            width: 8
                            height: 8
                            radius: 4
                            color: "#08090D"
                            border.color: "#4A5568"
                            border.width: 1
                        }
                    }
                }

                // Vinyl rotation when playing
                RotationAnimation on rotation {
                    running: !Mpv.paused && root.isAudio && !root.hasCover
                    loops: Animation.Infinite
                    from: 0; to: 360
                    duration: 20000
                }
            }

            MouseArea {
                id: posterMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Mpv.togglePause()
            }
        }

        // Track Information Typography Block
        Column {
            id: metaColumn
            anchors.top: posterArea.bottom
            anchors.topMargin: 20
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 6
            width: Math.min(520, root.width - 40)

            // Title
            Text {
                id: titleText
                anchors.horizontalCenter: parent.horizontalCenter
                text: Mpv.trackTitle || "No Media"
                color: "#FFFFFF"
                font.family: Theme.fontFamily
                font.pixelSize: root.cardSize > 240 ? 22 : 18
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                width: parent.width
            }

            // Artist
            Text {
                id: artistText
                anchors.horizontalCenter: parent.horizontalCenter
                text: Mpv.trackArtist || "Unknown Artist"
                color: "#A0A8BA"
                font.family: Theme.fontFamily
                font.pixelSize: root.cardSize > 240 ? 15 : 13
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                width: parent.width
            }

            // Album & Year (Clean, elegant, no ugly format badge!)
            Text {
                id: albumText
                anchors.horizontalCenter: parent.horizontalCenter
                visible: (Mpv.trackAlbum.length > 0 || Mpv.trackYear.length > 0)
                text: (Mpv.trackAlbum ? Mpv.trackAlbum : "") + (Mpv.trackYear ? ("  (" + Mpv.trackYear + ")") : "")
                color: "#646F84"
                font.family: Theme.fontFamily
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                width: parent.width
            }
        }
    }
}
