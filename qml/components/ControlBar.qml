import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import Parvaz 1.0
import ".."

Item {
    id: root

    property bool pointerOnBar: false
    property bool isFullscreen: false
    property bool playlistOpen: false
    property var blurSource: null
    property alias isDraggingSeek: seekBar.isDragging

    signal pokeRequested()
    signal openClicked()
    signal openUrlRequested()
    signal openSubClicked()
    signal searchSubtitlesRequested()
    signal fullscreenToggled()
    signal playlistToggleRequested()

    height: Theme.controlBarHeight

    // Frosted glass background: GPU blur of video + deep cinematic vignette gradient
    Item {
        id: dockBg
        anchors.fill: parent

        // GPU blur of the video region behind the dock
        MultiEffect {
            anchors.fill: parent
            source: root.blurSource
            visible: root.blurSource !== null && root.opacity > 0.0
            blurEnabled: true
            blur: 0.5
            blurMax: 32
            saturation: 0.15
            brightness: -0.15
        }

        // Cinematic gradient from soft shadow to solid deep graphite
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#00000000" }
                GradientStop { position: 0.25; color: "#8008090D" }
                GradientStop { position: 1.0; color: "#F208090D" }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 1
                color: Theme.borderGlassSubtle
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
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        anchors.topMargin: 6
        anchors.bottomMargin: 8
        spacing: 6

        // ---- 1. Hairline Integrated Timeline ----
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

        // ---- 2. Primary Playback Controls & Utility Actions ----
        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            height: 38

            // Left Section: Playback controls & Monospace time readout
            Row {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                // Hero Play/Pause button
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
                    tooltipText: Mpv.playlistCount > 1 ? "Previous track (Page Down)" : "Frame backward (Comma)"
                    buttonSize: 30
                    iconSize: 17
                    enabled: Mpv.playlistCount > 1 ? Mpv.hasPrevious : true
                    opacity: enabled ? 1.0 : 0.4
                    onClicked: {
                        if (Mpv.playlistCount > 1) {
                            Mpv.previous()
                        } else {
                            Mpv.frameStep(-1)
                        }
                    }
                }

                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "skip-forward"
                    tooltipText: Mpv.playlistCount > 1 ? "Next track (Page Up)" : "Frame forward (Period)"
                    buttonSize: 30
                    iconSize: 17
                    enabled: Mpv.playlistCount > 1 ? Mpv.hasNext : true
                    opacity: enabled ? 1.0 : 0.4
                    onClicked: {
                        if (Mpv.playlistCount > 1) {
                            Mpv.next()
                        } else {
                            Mpv.frameStep(1)
                        }
                    }
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
                    height: 14
                    color: Theme.borderGlass
                }

                // Clean time display (Position / Duration)
                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    Text {
                        text: Mpv.formatTime(Mpv.position)
                        color: "#FFFFFF"
                        font.family: Theme.fontDigits
                        font.pixelSize: Theme.fontMedium
                        font.bold: true
                    }

                    Text {
                        text: "/"
                        color: Theme.textMuted
                        font.family: Theme.fontDigits
                        font.pixelSize: Theme.fontNormal
                    }

                    Text {
                        text: Mpv.formatTime(Mpv.duration)
                        color: Theme.textSecondary
                        font.family: Theme.fontDigits
                        font.pixelSize: Theme.fontMedium
                    }
                }
            }

            // Right Section: Volume, Tracks, Speed, HW, Open, Fullscreen
            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                // Volume control
                VolumeControl {
                    anchors.verticalCenter: parent.verticalCenter
                    volume: Mpv.volume
                    muted: Mpv.muted
                    onVolumeChangeRequested: (v) => {
                        if (Mpv.muted && v > 0)
                            Mpv.setMuted(false);
                        Mpv.setVolume(v);
                        Mpv.showOsd("Volume: " + v + "%");
                    }
                    onMuteToggled: {
                        // Compute the new state BEFORE the call: Mpv.muted
                        // only updates through the async mpv echo.
                        var newMuted = !Mpv.muted
                        Mpv.setMuted(newMuted)
                        Mpv.showOsd(newMuted ? "Mute" : "Unmute (" + Mpv.volume + "%)")
                    }
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 1
                    height: 14
                    color: Theme.borderGlass
                }

                // Subtitle tracks button
                IconButton {
                    id: subsBtn
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "subtitles"
                    tooltipText: "Subtitles & Options (V / S)"
                    buttonSize: 30
                    iconSize: 17
                    active: (Mpv.subtitlesVisible && Mpv.currentSubtitleTrack > 0) || subsMenu.visible
                    onClicked: {
                        if (subsMenu.visible) {
                            subsMenu.close();
                        } else {
                            if (audioMenu.visible)
                                audioMenu.close();
                            subsMenu.open();
                        }
                    }
                }

                // Audio tracks button
                IconButton {
                    id: audioBtn
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "music"
                    tooltipText: "Audio tracks (A)"
                    buttonSize: 30
                    iconSize: 17
                    active: (Mpv.currentAudioTrack > 0 && !Mpv.muted) || audioMenu.visible
                    onClicked: {
                        if (audioMenu.visible) {
                            audioMenu.close();
                        } else {
                            if (subsMenu.visible)
                                subsMenu.close();
                            audioMenu.open();
                        }
                    }
                }

                // Clean text speed control (No bordered pill!)
                Item {
                    anchors.verticalCenter: parent.verticalCenter
                    width: speedRow.implicitWidth + 12
                    height: 28

                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radiusSmall
                        color: speedMouse.containsMouse ? Theme.bgGlassHover : "transparent"

                        Behavior on color { ColorAnimation { duration: Theme.animFast } }
                    }

                    Row {
                        id: speedRow
                        anchors.centerIn: parent
                        spacing: 2

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: Mpv.speed.toFixed(2) + "×"
                            color: speedMouse.containsMouse ? "#FFFFFF" : Theme.textSecondary
                            font.family: Theme.fontDigits
                            font.pixelSize: Theme.fontNormal
                            font.bold: true

                            Behavior on color { ColorAnimation { duration: Theme.animFast } }
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
                        text: "Speed (Z: 1.0x, X: -0.1x, C: +0.1x)"
                        delay: 400
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                    }
                }

                // Discrete HW Indicator (Clean dot + label)
                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 5
                        height: 5
                        radius: 2.5
                        color: Mpv.hwdec.length > 0 ? Theme.statusGreen : Theme.textMuted
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "HW"
                        color: Mpv.hwdec.length > 0 ? Theme.textSecondary : Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                // Playlist toggle button
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "list"
                    tooltipText: "Playlist (F6)"
                    buttonSize: 30
                    iconSize: 17
                    active: root.playlistOpen
                    onClicked: root.playlistToggleRequested()
                }

                // Open file button
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "folder-open"
                    tooltipText: "Open media…"
                    buttonSize: 30
                    iconSize: 17
                    onClicked: root.openClicked()
                }

                // Fullscreen toggle button
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: root.isFullscreen ? "minimize-2" : "maximize-2"
                    tooltipText: root.isFullscreen ? "Exit Fullscreen (F / Enter)" : "Fullscreen (F / Enter)"
                    buttonSize: 30
                    iconSize: 17
                    onClicked: root.fullscreenToggled()
                }
            }
        }
    }

    component ControlMenuItem: MenuItem {
        id: cItem
        implicitWidth: 240
        implicitHeight: 28
        padding: 0
        leftPadding: 6
        rightPadding: 6
        topPadding: 2
        bottomPadding: 2

        contentItem: Item {
            anchors.fill: parent

            Item {
                id: chkContainer
                anchors.left: parent.left
                anchors.leftMargin: 2
                anchors.verticalCenter: parent.verticalCenter
                width: 14
                height: 14
                visible: cItem.checkable

                Image {
                    anchors.centerIn: parent
                    width: 11
                    height: 11
                    source: "../icons/check.svg"
                    sourceSize: Qt.size(11, 11)
                    visible: cItem.checked
                }
            }

            Text {
                anchors.left: cItem.checkable ? chkContainer.right : parent.left
                anchors.leftMargin: cItem.checkable ? 6 : 6
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                text: cItem.text
                color: !cItem.enabled ? "#555A6B" : (cItem.highlighted ? "#FFFFFF" : "#E2E4EC")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontNormal
                elide: Text.ElideRight
            }
        }

        background: Rectangle {
            implicitWidth: 240
            implicitHeight: 28
            color: cItem.highlighted ? "#2A3348" : "transparent"
            radius: 3
        }
    }

    component ControlMenuSeparator: MenuSeparator {
        padding: 4
        topPadding: 4
        bottomPadding: 4
        contentItem: Rectangle {
            implicitHeight: 1
            implicitWidth: 226
            color: "#222736"
        }
    }

    // Audio Menu
    Menu {
        id: audioMenu
        parent: audioBtn
        x: Math.round((audioBtn.width - width) / 2)
        y: -height - 8

        background: Rectangle {
            implicitWidth: 240
            color: "#14161F"
            border.color: "#282D3E"
            border.width: 1
            radius: 4
        }

        Repeater {
            model: Mpv.audioTracks
            delegate: ControlMenuItem {
                text: modelData.label
                checkable: true
                checked: modelData.selected
                onTriggered: Mpv.selectAudioTrack(modelData.id)
            }
        }

        ControlMenuSeparator { visible: Mpv.audioTracks.length > 0 }

        ControlMenuItem {
            text: "Next audio track (A)"
            onTriggered: Mpv.cycleAudioTrack()
        }

        ControlMenuItem {
            text: "Night mode (Normalizer) (N)"
            checkable: true
            checked: Mpv.audioNormalizer
            onTriggered: Mpv.toggleAudioNormalizer()
        }
    }

    // Subtitle Menu
    Menu {
        id: subsMenu
        parent: subsBtn
        x: Math.round((subsBtn.width - width) / 2)
        y: -height - 8

        background: Rectangle {
            implicitWidth: 240
            color: "#14161F"
            border.color: "#282D3E"
            border.width: 1
            radius: 4
        }

        ControlMenuItem {
            text: "None (Disable subtitles)"
            checkable: true
            checked: Mpv.currentSubtitleTrack <= 0
            onTriggered: Mpv.selectSubtitleTrack(0)
        }

        ControlMenuSeparator { visible: Mpv.subTracks.length > 0 }

        Repeater {
            model: Mpv.subTracks
            delegate: ControlMenuItem {
                text: modelData.label
                checkable: true
                checked: modelData.selected
                onTriggered: Mpv.selectSubtitleTrack(modelData.id)
            }
        }

        ControlMenuSeparator {}

        ControlMenuItem {
            text: "Visible (V)"
            checkable: true
            checked: Mpv.subtitlesVisible
            onTriggered: {
                var newVisible = !Mpv.subtitlesVisible
                Mpv.setSubtitlesVisible(newVisible)
                Mpv.showOsd(newVisible ? "Subtitles: On" : "Subtitles: Off")
            }
        }

        ControlMenuItem {
            text: "Delay −0.1s ([)"
            onTriggered: {
                Mpv.setSubtitleDelay(Mpv.subtitleDelay - 0.1)
                Mpv.showOsd("Sub delay: " + Mpv.subtitleDelay.toFixed(1) + "s")
            }
        }

        ControlMenuItem {
            text: "Delay +0.1s (])"
            onTriggered: {
                Mpv.setSubtitleDelay(Mpv.subtitleDelay + 0.1)
                Mpv.showOsd("Sub delay: " + Mpv.subtitleDelay.toFixed(1) + "s")
            }
        }

        ControlMenuItem {
            text: "Font size − (Alt+PageDown)"
            onTriggered: Mpv.adjustSubtitleFontSize(-2)
        }

        ControlMenuItem {
            text: "Font size + (Alt+PageUp)"
            onTriggered: Mpv.adjustSubtitleFontSize(2)
        }

        ControlMenuSeparator {}

        ControlMenuItem {
            text: "Persian legacy encoding (cp1256)"
            onTriggered: Mpv.setSubtitleCodepage("cp1256")
        }

        ControlMenuItem {
            text: "UTF-8 encoding (default)"
            onTriggered: Mpv.setSubtitleCodepage("utf-8")
        }

        ControlMenuItem {
            text: "Open subtitle file…"
            onTriggered: root.openSubClicked()
        }

        ControlMenuItem {
            text: "Search online subtitles… (Ctrl+Shift+S)"
            onTriggered: root.searchSubtitlesRequested()
        }
    }
}
