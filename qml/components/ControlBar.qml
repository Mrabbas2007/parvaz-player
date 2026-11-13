import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import Parvaz 1.0
import ".."

Item {
    id: root

    property bool pointerOnBar: false
    property bool isFullscreen: false
    property var blurSource: null
    property alias isDraggingSeek: seekBar.isDragging

    signal pokeRequested()
    signal openClicked()
    signal openSubClicked()
    signal fullscreenToggled()

    height: 84

    // Frosted glass background: blurred backdrop + dark obsidian tint
    Item {
        id: dockBg
        anchors.fill: parent

        // GPU blur of the video region behind the dock
        MultiEffect {
            anchors.fill: parent
            source: root.blurSource
            visible: root.blurSource !== null && root.opacity > 0.0
            blurEnabled: true
            blur: 0.6
            blurMax: 32
            saturation: 0.25
            brightness: -0.15
        }

        // Semi-transparent obsidian glass layer with border highlight
        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusLarge
            color: root.blurSource !== null ? "#B8101218" : Theme.bgGlass
            border.color: Theme.borderGlass
            border.width: 1

            gradient: Gradient {
                GradientStop { position: 0.0; color: "#CC181B24" }
                GradientStop { position: 1.0; color: "#E0101218" }
            }
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
        spacing: 6

        // ---- 1. Full-width Seek Bar ----
        SeekBar {
            id: seekBar
            anchors.left: parent.left
            anchors.right: parent.right
            position: Mpv.position
            duration: Mpv.duration
            onSeekRequested: (target) => Mpv.seekAbsolute(target)
            onSeekingStarted: root.pokeRequested()
            onSeekingFinished: root.pokeRequested()
        }

        // ---- 2. Controls & Actions Row ----
        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            height: 38

            // Left Section: Playback controls & Time readout
            Row {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                // Play / Pause prominent button
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: Mpv.paused ? "play" : "pause"
                    tooltipText: Mpv.paused ? "Play (Space)" : "Pause (Space)"
                    buttonSize: 36
                    iconSize: 20
                    active: !Mpv.paused
                    onClicked: Mpv.togglePause()
                }

                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "skip-back"
                    tooltipText: "Frame backward (Comma)"
                    buttonSize: 32
                    iconSize: 18
                    onClicked: Mpv.frameStep(-1)
                }

                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "skip-forward"
                    tooltipText: "Frame forward (Period)"
                    buttonSize: 32
                    iconSize: 18
                    onClicked: Mpv.frameStep(1)
                }

                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "square"
                    tooltipText: "Stop to start (Backspace)"
                    buttonSize: 30
                    iconSize: 15
                    onClicked: { Mpv.seekAbsolute(0); if (!Mpv.paused) Mpv.togglePause() }
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 1
                    height: 18
                    color: Theme.borderGlassSubtle
                }

                // Time readout (Position / Duration)
                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    Text {
                        text: Mpv.formatTime(Mpv.position)
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontNormal
                        font.bold: true
                    }

                    Text {
                        text: "/"
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontNormal
                    }

                    Text {
                        text: Mpv.formatTime(Mpv.duration)
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontNormal
                    }
                }
            }

            // Right Section: Volume, Tracks, Speed, Open, Fullscreen
            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                // Volume control
                VolumeControl {
                    anchors.verticalCenter: parent.verticalCenter
                    volume: Mpv.volume
                    muted: Mpv.muted
                    onVolumeChangeRequested: (v) => Mpv.setVolume(v)
                    onMuteToggled: Mpv.setMuted(!Mpv.muted)
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 1
                    height: 18
                    color: Theme.borderGlassSubtle
                }

                // Subtitle tracks button
                IconButton {
                    id: subsBtn
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "subtitles"
                    tooltipText: "Subtitles & Options (V / S)"
                    buttonSize: 32
                    iconSize: 18
                    active: Mpv.subtitlesVisible
                    onClicked: subsMenu.open()
                }

                // Audio tracks button
                IconButton {
                    id: audioBtn
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "music"
                    tooltipText: "Audio tracks (A)"
                    buttonSize: 32
                    iconSize: 18
                    onClicked: audioMenu.open()
                }

                // Speed pill badge
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    height: 28
                    width: speedRow.implicitWidth + 16
                    radius: Theme.radiusSmall
                    color: speedMouse.containsMouse ? Theme.bgGlassHover : Theme.bgGlassLight
                    border.color: Theme.borderGlassSubtle
                    border.width: 1

                    Row {
                        id: speedRow
                        anchors.centerIn: parent
                        spacing: 5

                        Image {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 14
                            height: 14
                            source: "../icons/gauge.svg"
                            sourceSize: Qt.size(28, 28)
                            fillMode: Image.PreserveAspectFit
                            opacity: 0.85
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: Mpv.speed.toFixed(2) + "x"
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall
                            font.bold: true
                        }
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
                        text: "Playback speed (Z: 1.0x, X: -0.1x, C: +0.1x)"
                        delay: 400
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                    }
                }

                // Hardware decode pill
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    height: 24
                    width: hwText.implicitWidth + 12
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

                // Open file button
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "folder-open"
                    tooltipText: "Open media…"
                    buttonSize: 32
                    iconSize: 18
                    onClicked: root.openClicked()
                }

                // Fullscreen toggle button
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: root.isFullscreen ? "minimize-2" : "maximize-2"
                    tooltipText: root.isFullscreen ? "Exit Fullscreen (F / Enter)" : "Fullscreen (F / Enter)"
                    buttonSize: 32
                    iconSize: 18
                    onClicked: root.fullscreenToggled()
                }
            }
        }
    }

    // Audio Menu
    Menu {
        id: audioMenu
        x: audioBtn.x - 60
        y: audioBtn.y - height - 8

        background: Rectangle {
            implicitWidth: 210
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
        x: subsBtn.x - 80
        y: subsBtn.y - height - 8

        background: Rectangle {
            implicitWidth: 230
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
