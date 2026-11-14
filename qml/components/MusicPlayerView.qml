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

    // Ambient background atmosphere
    Rectangle {
        anchors.fill: parent
        color: "#0B0D13"

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
                GradientStop { position: 0.0; color: "#CC0B0D13" }
                GradientStop { position: 0.5; color: "#B30B0D13" }
                GradientStop { position: 1.0; color: "#F20B0D13" }
            }
        }
    }

    // Main Center Content
    Item {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -24
        width: Math.max(root.cardSize, metaColumn.implicitWidth)
        height: root.cardSize + metaColumn.implicitHeight + 20

        // Poster / Album Card Container
        Item {
            id: posterArea
            anchors.horizontalCenter: parent.horizontalCenter
            width: root.cardSize
            height: root.cardSize

            // Outer Soft Glow Shadow
            Rectangle {
                anchors.fill: parent
                radius: 12
                color: "#203B82F6"
                visible: root.hasCover
                opacity: 0.4
                scale: 1.03
                z: 0
            }

            // The Cover Art Card (When cover image is available)
            Rectangle {
                id: coverCard
                anchors.fill: parent
                radius: 12
                color: "#141722"
                border.color: "#283246"
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

            // Fallback: Vinyl Disc (When no cover image is available)
            Item {
                id: vinylDisc
                anchors.fill: parent
                visible: !root.hasCover
                z: 1

                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: "#10121A"
                    border.color: "#232838"
                    border.width: 2

                    // Subtle vinyl concentric grooves
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 0.82
                        height: width
                        radius: width / 2
                        color: "transparent"
                        border.color: "#181B26"
                        border.width: 1.5
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 0.65
                        height: width
                        radius: width / 2
                        color: "transparent"
                        border.color: "#1C202E"
                        border.width: 1.5
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 0.48
                        height: width
                        radius: width / 2
                        color: "transparent"
                        border.color: "#222738"
                        border.width: 1.5
                    }

                    // Center Vinyl Label
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 0.36
                        height: width
                        radius: width / 2
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#2563EB" }
                            GradientStop { position: 1.0; color: "#7C3AED" }
                        }
                        border.color: "#3B82F6"
                        border.width: 1

                        Image {
                            anchors.centerIn: parent
                            width: 28
                            height: 28
                            source: "../icons/music.svg"
                            sourceSize: Qt.size(28, 28)
                        }

                        // Center Spindle Hole
                        Rectangle {
                            anchors.centerIn: parent
                            width: 10
                            height: 10
                            radius: 5
                            color: "#0B0D13"
                            border.color: "#30394D"
                            border.width: 1
                        }
                    }
                }

                // Vinyl rotation when playing
                RotationAnimation on rotation {
                    running: !Mpv.paused && root.isAudio && !root.hasCover
                    loops: Animation.Infinite
                    from: 0; to: 360
                    duration: 22000
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
            anchors.topMargin: 18
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 5
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
                color: "#9CA7BA"
                font.family: Theme.fontFamily
                font.pixelSize: root.cardSize > 240 ? 15 : 13
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                width: parent.width
            }

            // Album & Year
            Text {
                id: albumText
                anchors.horizontalCenter: parent.horizontalCenter
                visible: (Mpv.trackAlbum.length > 0 || Mpv.trackYear.length > 0)
                text: (Mpv.trackAlbum ? Mpv.trackAlbum : "") + (Mpv.trackYear ? ("  (" + Mpv.trackYear + ")") : "")
                color: "#606C82"
                font.family: Theme.fontFamily
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                width: parent.width
            }

            // Format Badge (Pill)
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.topMargin: 4
                implicitWidth: badgeText.implicitWidth + 14
                implicitHeight: 20
                radius: 10
                color: "#182030"
                border.color: "#2C3952"
                border.width: 1

                Text {
                    id: badgeText
                    anchors.centerIn: parent
                    text: {
                        var ext = "AUDIO";
                        if (Mpv.currentFile.length > 0) {
                            var parts = Mpv.currentFile.split('.');
                            if (parts.length > 1) ext = parts[parts.length - 1].toUpperCase();
                        }
                        return ext + (Mpv.audioNormalizer ? " • NIGHT" : "");
                    }
                    color: "#7E8DA6"
                    font.family: Theme.fontDigits
                    font.pixelSize: 10
                    font.bold: true
                }
            }
        }
    }
}
