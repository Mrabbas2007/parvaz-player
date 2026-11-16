import QtQuick
import QtQuick.Controls
import Parvaz 1.0
import ".."

Item {
    id: root

    property bool chromeVisible: true
    property bool isFullscreen: false
    signal pokeRequested()

    readonly property bool isAudio: Mpv.isAudioOnly
    readonly property bool hasCover: Mpv.coverArtUrl.length > 0
    readonly property real cardSize: Math.max(160, Math.min(320, Math.min(parent.width - 60, parent.height - 230)))

    visible: isAudio
    opacity: isAudio ? 1.0 : 0.0
    Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }

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

    // Main Center Content
    Item {
        id: mainContent
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

            // Subtle Depth Shadow for Cover Card
            Rectangle {
                anchors.fill: parent
                radius: 16
                color: "#000000"
                opacity: root.hasCover ? 0.45 : 0.0
                anchors.topMargin: 4
                anchors.bottomMargin: -4
                z: 0
                visible: root.hasCover
            }

            // The Cover Art Card (When Cover is Available)
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
                    opacity: (posterMouse.containsMouse && (root.chromeVisible || !root.isFullscreen)) ? 1.0 : 0.0
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

            // Fallback: Studio Obsidian Vinyl Disc (When No Cover Exists)
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
                cursorShape: (!root.chromeVisible && root.isFullscreen) ? Qt.BlankCursor : Qt.PointingHandCursor
                onPositionChanged: root.pokeRequested()
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
