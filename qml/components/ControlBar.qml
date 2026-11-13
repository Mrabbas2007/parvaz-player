import QtQuick
import QtQuick.Controls
import Parvaz 1.0
import ".."

Item {
    id: root

    property bool pointerOnBar: false
    property alias isDraggingSeek: seekBar.isDragging

    signal pokeRequested()
    signal openClicked()
    signal openSubClicked()
    signal fullscreenToggled()

    height: Theme.controlBarHeight

    // Translucent glass dock background
    Rectangle {
        anchors.fill: parent
        color: Theme.bgGlass
        border.color: Theme.borderGlass
        border.width: 1
        radius: Theme.radiusLarge

        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.bgGlassLight }
            GradientStop { position: 1.0; color: Theme.bgGlass }
        }
    }

    // Pointer detection over bar
    MouseArea {
        id: barMouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        onEntered: { root.pointerOnBar = true }
        onExited: { root.pointerOnBar = false }
        onPositionChanged: root.pokeRequested()
    }

    Column {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 8
        anchors.bottomMargin: 8
        spacing: 4

        // ---- Seekbar Row (Current time + SeekBar + Duration) ----
        Row {
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 12
            height: 24

            Text {
                id: timeLabel
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
                font.bold: true
                text: Mpv.formatTime(Mpv.position)
            }

            SeekBar {
                id: seekBar
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - timeLabel.width - durationLabel.width - 24
                position: Mpv.position
                duration: Mpv.duration
                onSeekRequested: (target) => Mpv.seekAbsolute(target)
                onSeekingStarted: root.pokeRequested()
                onSeekingFinished: root.pokeRequested()
            }

            Text {
                id: durationLabel
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
                text: Mpv.formatTime(Mpv.duration)
            }
        }

        // ---- Controls Row ----
        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            height: 36

            // Left Section (Open, Audio, Subs)
            Row {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                IconButton {
                    iconName: "open"
                    tooltipText: "Open media…"
                    buttonSize: 32
                    iconSize: 15
                    onClicked: root.openClicked()
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 1; height: 16; color: Theme.borderGlassSubtle
                }

                IconButton {
                    id: audioBtn
                    iconName: "audio"
                    tooltipText: "Audio tracks"
                    buttonSize: 32
                    iconSize: 15
                    onClicked: audioMenu.open()
                }

                IconButton {
                    id: subsBtn
                    iconName: "subtitles"
                    tooltipText: "Subtitle tracks & options"
                    buttonSize: 32
                    iconSize: 15
                    onClicked: subsMenu.open()
                }
            }

            // Center Section (Prev, Play/Pause, Next, Stop)
            Row {
                anchors.centerIn: parent
                spacing: 8

                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "step-back"
                    tooltipText: "Frame backward (Comma)"
                    buttonSize: 32
                    iconSize: 14
                    onClicked: Mpv.frameStep(-1)
                }

                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: Mpv.paused ? "play" : "pause"
                    tooltipText: Mpv.paused ? "Play (Space)" : "Pause (Space)"
                    buttonSize: 36
                    iconSize: 17
                    active: true
                    onClicked: Mpv.togglePause()
                }

                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "step-forward"
                    tooltipText: "Frame forward (Period)"
                    buttonSize: 32
                    iconSize: 14
                    onClicked: Mpv.frameStep(1)
                }

                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "stop"
                    tooltipText: "Stop to start (Backspace)"
                    buttonSize: 32
                    iconSize: 13
                    onClicked: { Mpv.seekAbsolute(0); if (!Mpv.paused) Mpv.togglePause() }
                }
            }

            // Right Section (Volume, Speed, HW, Fullscreen)
            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                VolumeControl {
                    anchors.verticalCenter: parent.verticalCenter
                    volume: Mpv.volume
                    muted: Mpv.muted
                    onVolumeChangeRequested: (v) => Mpv.setVolume(v)
                    onMuteToggled: Mpv.setMuted(!Mpv.muted)
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 1; height: 16; color: Theme.borderGlassSubtle
                }

                // Speed indicator capsule
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: speedText.implicitWidth + 16
                    height: 26
                    radius: Theme.radiusSmall
                    color: speedMouse.containsMouse ? Theme.bgGlassHover : Theme.bgGlassLight
                    border.color: Theme.borderGlassSubtle
                    border.width: 1

                    Text {
                        id: speedText
                        anchors.centerIn: parent
                        text: Mpv.speed.toFixed(2) + "x"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                    }

                    MouseArea {
                        id: speedMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton
                        onClicked: {
                            if (Mpv.speed < 0.75)
                                Mpv.setSpeed(1.0)
                            else if (Mpv.speed < 1.5)
                                Mpv.setSpeed(2.0)
                            else
                                Mpv.setSpeed(0.5)
                        }
                    }

                    ToolTip {
                        visible: speedMouse.containsMouse
                        text: "Playback speed (Z/X/C)"
                        delay: 500
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                    }
                }

                // HW decode pill
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: hwText.implicitWidth + 12
                    height: 22
                    radius: Theme.radiusSmall
                    color: Theme.bgGlassLight
                    border.color: Theme.borderGlassSubtle
                    border.width: 1

                    Text {
                        id: hwText
                        anchors.centerIn: parent
                        text: "hw: " + (Mpv.hwdec.length > 0 ? Mpv.hwdec : "off")
                        color: Mpv.hwdec.length > 0 ? Theme.statusGreen : Theme.statusRed
                        font.family: Theme.fontFamily
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: root.parent.visibility === Window.FullScreen ? "fullscreen-exit" : "fullscreen"
                    tooltipText: "Fullscreen (Enter / F)"
                    buttonSize: 32
                    iconSize: 15
                    onClicked: root.fullscreenToggled()
                }
            }
        }
    }

    // Audio Menu
    Menu {
        id: audioMenu
        x: audioBtn.x
        y: audioBtn.y - height - 8

        background: Rectangle {
            implicitWidth: 200
            color: Theme.bgGlass
            border.color: Theme.borderGlass
            border.width: 1
            radius: Theme.radiusMedium
        }

        Repeater {
            model: Mpv.tracksOfType("audio")
            delegate: MenuItem {
                text: modelData.label
                checkable: true
                checked: modelData.selected
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontNormal
                onTriggered: Mpv.selectTrack(modelData.id)
            }
        }

        MenuSeparator {}

        MenuItem {
            text: "Next audio track (A)"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontNormal
            onTriggered: Mpv.cycleAudioTrack()
        }
    }

    // Subtitle Menu
    Menu {
        id: subsMenu
        x: subsBtn.x
        y: subsBtn.y - height - 8

        background: Rectangle {
            implicitWidth: 220
            color: Theme.bgGlass
            border.color: Theme.borderGlass
            border.width: 1
            radius: Theme.radiusMedium
        }

        Repeater {
            model: Mpv.tracksOfType("sub")
            delegate: MenuItem {
                text: modelData.label
                checkable: true
                checked: modelData.selected
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontNormal
                onTriggered: Mpv.selectTrack(modelData.id)
            }
        }

        MenuItem {
            text: "Disable subtitles"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontNormal
            onTriggered: Mpv.disableSubtitles()
        }

        MenuSeparator {}

        MenuItem {
            text: "Visible (V)"
            checkable: true
            checked: Mpv.subtitlesVisible
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontNormal
            onTriggered: {
                Mpv.setSubtitlesVisible(!Mpv.subtitlesVisible)
                Mpv.showOsd(Mpv.subtitlesVisible ? "Subtitles: On" : "Subtitles: Off")
            }
        }

        MenuItem {
            text: "Delay −0.1s ([)"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontNormal
            onTriggered: {
                Mpv.setSubtitleDelay(Mpv.subtitleDelay - 0.1)
                Mpv.showOsd("Sub delay: " + Mpv.subtitleDelay.toFixed(1) + "s")
            }
        }

        MenuItem {
            text: "Delay +0.1s (])"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontNormal
            onTriggered: {
                Mpv.setSubtitleDelay(Mpv.subtitleDelay + 0.1)
                Mpv.showOsd("Sub delay: " + Mpv.subtitleDelay.toFixed(1) + "s")
            }
        }

        MenuItem {
            text: "Font size − (Alt+PageDown)"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontNormal
            onTriggered: Mpv.adjustSubtitleFontSize(-2)
        }

        MenuItem {
            text: "Font size + (Alt+PageUp)"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontNormal
            onTriggered: Mpv.adjustSubtitleFontSize(2)
        }

        MenuSeparator {}

        MenuItem {
            text: "Persian legacy encoding (cp1256)"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontNormal
            onTriggered: Mpv.setSubtitleCodepage("cp1256")
        }

        MenuItem {
            text: "UTF-8 encoding (default)"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontNormal
            onTriggered: Mpv.setSubtitleCodepage("utf-8")
        }

        MenuItem {
            text: "Open subtitle file…"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontNormal
            onTriggered: root.openSubClicked()
        }
    }
}
