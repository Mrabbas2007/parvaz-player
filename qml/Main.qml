import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Parvaz 1.0

Window {
    id: root
    width: 1280
    height: 720
    minimumWidth: 320
    minimumHeight: 180
    visible: true
    color: "black"
    title: Mpv.mediaTitle.length > 0 ? Mpv.mediaTitle : "Parvaz"

    // Auto-hide: 2s of no pointer motion hides the bar, but only while playing.
    // It never hides while paused, or while the pointer rests on the bar.
    Timer {
        id: hideTimer
        interval: 2000
        repeat: false
        onTriggered: if (!Mpv.paused && !root.pointerOnBar) root.barVisible = false
    }

    property bool barVisible: true
    property bool pointerOnBar: false
    property bool alwaysOnTop: false
    flags: alwaysOnTop ? (Qt.Window | Qt.WindowStaysOnTopHint) : Qt.Window

    function toggleAlwaysOnTop() {
        alwaysOnTop = !alwaysOnTop
        Mpv.showOsd(alwaysOnTop ? "Always on top: On" : "Always on top: Off")
    }

    function poke() {
        barVisible = true
        hideTimer.restart()
    }

    function toggleFullscreen() {
        root.visibility = (root.visibility === Window.FullScreen
                           ? Window.Windowed : Window.FullScreen)
    }

    // MouseArea::containsMouse is only reliable while the item is visible, so
    // this is a cheap way to keep the flag honest across hide/show cycles.
    onBarVisibleChanged: {
        if (barVisible) {
            poke()
        } else {
            hideTimer.stop()
            pointerOnBar = false
        }
        Mpv.setBottomBarOffset(barVisible, bar.height, root.height)
    }

    onHeightChanged: {
        Mpv.setBottomBarOffset(barVisible, bar.height, root.height)
    }

    // Restart the countdown when playback resumes, otherwise a resume with no
    // mouse motion would leave the bar on screen forever.
    Connections {
        target: Mpv
        function onPausedChanged() {
            if (!Mpv.paused)
                poke()
        }
    }

    Component.onCompleted: {
        poke()
        Mpv.setBottomBarOffset(barVisible, bar.height, root.height)
    }

    function localPath(u) {
        var s = u.toString()
        if (s.startsWith("file://"))
            s = s.substring(7)
        return decodeURIComponent(s)
    }

    FileDialog {
        id: openDialog
        title: "Open media"
        nameFilters: [
            "Media (*.mkv *.mp4 *.webm *.avi *.mov *.flv *.ts *.ogm *.mp3 *.flac *.ogg *.opus *.wav)",
            "All files (*)"
        ]
        onAccepted: Mpv.loadFile(root.localPath(openDialog.selectedFile))
    }

    FileDialog {
        id: subDialog
        title: "Open subtitle file"
        nameFilters: [
            "Subtitles (*.srt *.ass *.ssa *.sub *.vtt *.sup)",
            "All files (*)"
        ]
        onAccepted: Mpv.addSubtitleFile(root.localPath(subDialog.selectedFile))
    }

    MpvFboItem {
        id: video
        anchors.fill: parent
    }

    // Mouse motion anywhere over the video wakes the bar. hoverEnabled is
    // required: without it onPositionChanged only fires while a button is held.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        hoverEnabled: true
        onPositionChanged: poke()
        onClicked: (mouse) => {
            if (mouse.button === Qt.LeftButton) {
                Mpv.togglePause()
            } else if (mouse.button === Qt.RightButton) {
                contextMenu.popup()
            }
        }
        onDoubleClicked: (mouse) => {
            if (mouse.button === Qt.LeftButton)
                root.visibility = (root.visibility === Window.FullScreen
                                   ? Window.Windowed : Window.FullScreen)
        }
    }

    // --- control bar -------------------------------------------------------
    Rectangle {
        id: bar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 76
        color: "#cc000000"
        opacity: root.barVisible ? 1.0 : 0.0
        visible: opacity > 0.0
        Behavior on opacity { NumberAnimation { duration: 140 } }

        // The pointer sitting on the bar keeps it open. Clicks on the bar's own
        // empty space must not reach the video MouseArea underneath.
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.AllButtons
            onEntered: { root.pointerOnBar = true;  hideTimer.stop() }
            onExited:  { root.pointerOnBar = false; if (!Mpv.paused) poke() }
            onPositionChanged: poke()
        }

        Column {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            anchors.topMargin: 8
            anchors.bottomMargin: 8
            spacing: 4

            // ---- seekbar row (current time + slider + duration) ----
            Row {
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 10

                Text {
                    id: timeLabel
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#f0f0f0"
                    font.pixelSize: 13
                    text: Mpv.formatTime(Mpv.position)
                }

                Slider {
                    id: seek
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - timeLabel.width - durationLabel.width - 40
                    from: 0
                    to: Math.max(0.001, Mpv.duration)
                    // While dragging, follow the finger; otherwise follow mpv.
                    property bool dragging: false
                    property real dragValue: 0
                    value: dragging ? dragValue : Mpv.position

                    onPressedChanged: {
                        if (pressed) {
                            // Capture the real position BEFORE flipping the
                            // binding: once `dragging` is true, `value` reads
                            // dragValue, so `dragValue = value` would capture
                            // the previous drag's value (0 on the first one).
                            dragValue = Mpv.position
                            dragging = true
                            hideTimer.stop()
                        } else {
                            dragging = false
                            poke()
                        }
                    }
                    onMoved: {
                        dragValue = value
                        Mpv.seekAbsolute(value)
                    }
                }

                Text {
                    id: durationLabel
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#f0f0f0"
                    font.pixelSize: 13
                    text: Mpv.formatTime(Mpv.duration)
                }
            }

            // ---- button row ----
            Row {
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 10

                Button {
                    text: "Open…"
                    // Must not take keyboard focus from the player shortcuts.
                    focusPolicy: Qt.NoFocus
                    onClicked: openDialog.open()
                }

                Button {
                    id: playPause
                    text: Mpv.paused ? "▶" : "⏸"
                    onClicked: Mpv.togglePause()
                }

                Button {
                    text: "<<"
                    onClicked: Mpv.frameStep(-1)
                }

                Button {
                    text: ">>"
                    onClicked: Mpv.frameStep(1)
                }

                // Both menus are declared as children of this Row so their x/y
                // share its coordinate space. Qt flips them up to stay on screen,
                // since the bar sits at the bottom edge.
                Button {
                    id: audioBtn
                    text: "Audio"
                    focusPolicy: Qt.NoFocus
                    onClicked: audioMenu.open()
                }

                Button {
                    id: subsBtn
                    text: "Subs"
                    focusPolicy: Qt.NoFocus
                    onClicked: subsMenu.open()
                }

                Menu {
                    id: audioMenu
                    x: audioBtn.x
                    y: audioBtn.y + audioBtn.height

                    Repeater {
                        model: Mpv.tracksOfType("audio")
                        delegate: MenuItem {
                            text: modelData.label
                            checkable: true
                            checked: modelData.selected
                            onTriggered: Mpv.selectTrack(modelData.id)
                        }
                    }

                    MenuSeparator {}

                    MenuItem {
                        text: "Next audio track"
                        onTriggered: Mpv.cycleAudioTrack()
                    }
                }

                Menu {
                    id: subsMenu
                    x: subsBtn.x
                    y: subsBtn.y + subsBtn.height

                    Repeater {
                        model: Mpv.tracksOfType("sub")
                        delegate: MenuItem {
                            text: modelData.label
                            checkable: true
                            checked: modelData.selected
                            onTriggered: Mpv.selectTrack(modelData.id)
                        }
                    }

                    MenuItem {
                        text: "Off"
                        onTriggered: Mpv.disableSubtitles()
                    }

                    MenuSeparator {}

                    MenuItem {
                        text: "Delay −0.1s"
                        onTriggered: Mpv.setSubtitleDelay(Mpv.subtitleDelay - 0.1)
                    }

                    MenuItem {
                        text: "Delay +0.1s"
                        onTriggered: Mpv.setSubtitleDelay(Mpv.subtitleDelay + 0.1)
                    }

                    MenuItem {
                        text: "Visible"
                        checkable: true
                        checked: Mpv.subtitlesVisible
                        onTriggered: Mpv.setSubtitlesVisible(!Mpv.subtitlesVisible)
                    }

                    MenuItem {
                        text: "Font size −"
                        onTriggered: Mpv.setSubtitleFontSize(Mpv.subtitleFontSize - 2)
                    }

                    MenuItem {
                        text: "Font size +"
                        onTriggered: Mpv.setSubtitleFontSize(Mpv.subtitleFontSize + 2)
                    }

                    MenuSeparator {}

                    MenuItem {
                        text: "Legacy encoding (cp1256)"
                        onTriggered: Mpv.setSubtitleCodepage("cp1256")
                    }

                    MenuItem {
                        text: "Open subtitle file…"
                        onTriggered: subDialog.open()
                    }
                }

                Button {
                    id: muteBtn
                    text: Mpv.muted ? "Unmute" : "Mute"
                    onClicked: Mpv.setMuted(!Mpv.muted)
                }

                Slider {
                    id: volume
                    anchors.verticalCenter: parent.verticalCenter
                    width: 110
                    from: 0
                    to: 100
                    stepSize: 1
                    value: Mpv.volume
                    onMoved: Mpv.setVolume(Math.round(value))
                }

                Item { width: 1; height: 1 }

                // Cycles 1.0x -> 2.0x -> 0.5x -> 1.0x based on the live speed,
                // so the first click from the default never goes backwards.
                Button {
                    id: speedBtn
                    text: Mpv.speed.toFixed(2) + "x"
                    onClicked: {
                        if (Mpv.speed < 0.75)
                            Mpv.setSpeed(1.0)
                        else if (Mpv.speed < 1.5)
                            Mpv.setSpeed(2.0)
                        else
                            Mpv.setSpeed(0.5)
                    }
                }

                Item { width: 1; height: 1 }

                Button {
                    text: "Fullscreen"
                    onClicked: root.toggleFullscreen()
                }

                Item { width: 1; height: 1 }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: Mpv.hwdec.length > 0 ? "#8fd18f" : "#d18f8f"
                    font.pixelSize: 12
                    text: "hw: " + (Mpv.hwdec.length > 0 ? Mpv.hwdec : "none")
                }
            }
        }
    }

    Text {
        visible: Mpv.duration <= 0
        anchors.centerIn: parent
        color: "#666666"
        font.pixelSize: 16
        text: "Drop a file here · Space = play/pause · ←/→ = 5s · F = fullscreen"
    }

    DropArea {
        anchors.fill: parent
        onDropped: (drop) => {
            if (drop.hasUrls && drop.urls.length > 0)
                Mpv.loadFile(root.localPath(drop.urls[0]))
        }
    }

    // --- keyboard ---------------------------------------------------------
    // All shortcuts are suspended while the file dialog is open, so the dialog
    // never leaves Space/arrows/F/Esc/Ctrl+Q bound to the player.
    component PlayerShortcut: Shortcut {
        enabled: !openDialog.visible && !subDialog.visible
    }

    // Playback
    PlayerShortcut { sequence: "Space"; onActivated: Mpv.togglePause() }
    PlayerShortcut { sequence: "Comma"; onActivated: Mpv.frameStep(-1) }
    PlayerShortcut { sequence: "Period"; onActivated: Mpv.frameStep(1) }

    // Seeking (PotPlayer style: Left/Right 5s, Ctrl 30s, Shift 60s, Ctrl+Shift 300s, Backspace/Home 0s)
    PlayerShortcut { sequence: "Left"; onActivated: Mpv.seekRelative(-5) }
    PlayerShortcut { sequence: "Right"; onActivated: Mpv.seekRelative(5) }
    PlayerShortcut { sequence: "Ctrl+Left"; onActivated: Mpv.seekRelative(-30) }
    PlayerShortcut { sequence: "Ctrl+Right"; onActivated: Mpv.seekRelative(30) }
    PlayerShortcut { sequence: "Shift+Left"; onActivated: Mpv.seekRelative(-60) }
    PlayerShortcut { sequence: "Shift+Right"; onActivated: Mpv.seekRelative(60) }
    PlayerShortcut { sequence: "Ctrl+Shift+Left"; onActivated: Mpv.seekRelative(-300) }
    PlayerShortcut { sequence: "Ctrl+Shift+Right"; onActivated: Mpv.seekRelative(300) }
    PlayerShortcut { sequence: "Backspace"; onActivated: { Mpv.seekAbsolute(0); Mpv.showOsd("00:00:00") } }
    PlayerShortcut { sequence: "Home"; onActivated: { Mpv.seekAbsolute(0); Mpv.showOsd("00:00:00") } }

    // Speed control (PotPlayer: Z = normal 1.0x, X = -0.1x, C = +0.1x)
    PlayerShortcut { sequence: "Z"; onActivated: Mpv.resetSpeed() }
    PlayerShortcut { sequence: "X"; onActivated: Mpv.adjustSpeed(-0.1) }
    PlayerShortcut { sequence: "C"; onActivated: Mpv.adjustSpeed(0.1) }

    // Volume (PotPlayer: Up/Down arrow = +/- 5%, M = mute)
    PlayerShortcut { sequence: "Up"; onActivated: { Mpv.adjustVolume(5); poke() } }
    PlayerShortcut { sequence: "Down"; onActivated: { Mpv.adjustVolume(-5); poke() } }
    PlayerShortcut { sequence: "Ctrl+Up"; onActivated: { Mpv.adjustVolume(5); poke() } }
    PlayerShortcut { sequence: "Ctrl+Down"; onActivated: { Mpv.adjustVolume(-5); poke() } }
    PlayerShortcut {
        sequence: "M"
        onActivated: {
            Mpv.setMuted(!Mpv.muted)
            Mpv.showOsd(Mpv.muted ? "Mute" : "Unmute (" + Mpv.volume + "%)")
        }
    }

    // Window sizing & Fullscreen (PotPlayer: 1=0.5x, 2=1.0x, 3=1.5x, 4=2.0x, Enter/F=Fullscreen)
    PlayerShortcut { sequence: "1"; onActivated: { root.visibility = Window.Windowed; root.width = 640; root.height = 360 } }
    PlayerShortcut { sequence: "2"; onActivated: { root.visibility = Window.Windowed; root.width = 1280; root.height = 720 } }
    PlayerShortcut { sequence: "3"; onActivated: { root.visibility = Window.Windowed; root.width = 1600; root.height = 900 } }
    PlayerShortcut { sequence: "4"; onActivated: { root.visibility = Window.Windowed; root.width = 1920; root.height = 1080 } }
    PlayerShortcut { sequence: "Enter"; onActivated: root.toggleFullscreen() }
    PlayerShortcut { sequence: "Return"; onActivated: root.toggleFullscreen() }
    PlayerShortcut { sequence: "F"; onActivated: root.toggleFullscreen() }
    PlayerShortcut { sequence: "Escape"; onActivated: if (root.visibility === Window.FullScreen) root.visibility = Window.Windowed }

    // Subtitles & Audio tracks
    PlayerShortcut { sequence: "S"; onActivated: Mpv.cycleSubtitleTrack() }
    PlayerShortcut { sequence: "A"; onActivated: Mpv.cycleAudioTrack() }
    PlayerShortcut {
        sequence: "V"
        onActivated: {
            Mpv.setSubtitlesVisible(!Mpv.subtitlesVisible)
            Mpv.showOsd(Mpv.subtitlesVisible ? "Subtitles: On" : "Subtitles: Off")
        }
    }
    PlayerShortcut {
        sequence: "["
        onActivated: {
            Mpv.setSubtitleDelay(Mpv.subtitleDelay - 0.1)
            Mpv.showOsd("Sub delay: " + Mpv.subtitleDelay.toFixed(1) + "s")
        }
    }
    PlayerShortcut {
        sequence: "]"
        onActivated: {
            Mpv.setSubtitleDelay(Mpv.subtitleDelay + 0.1)
            Mpv.showOsd("Sub delay: " + Mpv.subtitleDelay.toFixed(1) + "s")
        }
    }
    PlayerShortcut { sequence: "Alt+Up"; onActivated: Mpv.adjustSubtitlePosition(-2) }
    PlayerShortcut { sequence: "Alt+Down"; onActivated: Mpv.adjustSubtitlePosition(2) }
    PlayerShortcut { sequence: "Alt+PageUp"; onActivated: Mpv.adjustSubtitleFontSize(2) }
    PlayerShortcut { sequence: "Alt+PageDown"; onActivated: Mpv.adjustSubtitleFontSize(-2) }

    PlayerShortcut { sequence: "Ctrl+T"; onActivated: root.toggleAlwaysOnTop() }
    PlayerShortcut { sequence: "Ctrl+Q"; onActivated: Qt.quit() }

    // --- Right-click Context Menu (PotPlayer style) -----------------------
    Menu {
        id: contextMenu

        // 1. Open
        Menu {
            title: "Open"
            MenuItem {
                text: "Open media…"
                onTriggered: openDialog.open()
            }
            MenuItem {
                text: "Open subtitle file…"
                onTriggered: subDialog.open()
            }
        }

        MenuSeparator {}

        // 2. Playback
        Menu {
            title: "Playback"
            MenuItem {
                text: Mpv.paused ? "Play" : "Pause"
                onTriggered: Mpv.togglePause()
            }
            MenuItem {
                text: "Stop (to beginning)"
                onTriggered: { Mpv.seekAbsolute(0); if (!Mpv.paused) Mpv.togglePause() }
            }
            MenuSeparator {}
            Menu {
                title: "Jump forward"
                MenuItem { text: "+5s (Right)"; onTriggered: Mpv.seekRelative(5) }
                MenuItem { text: "+30s (Ctrl+Right)"; onTriggered: Mpv.seekRelative(30) }
                MenuItem { text: "+1m (Shift+Right)"; onTriggered: Mpv.seekRelative(60) }
                MenuItem { text: "+5m (Ctrl+Shift+Right)"; onTriggered: Mpv.seekRelative(300) }
            }
            Menu {
                title: "Jump backward"
                MenuItem { text: "-5s (Left)"; onTriggered: Mpv.seekRelative(-5) }
                MenuItem { text: "-30s (Ctrl+Left)"; onTriggered: Mpv.seekRelative(-30) }
                MenuItem { text: "-1m (Shift+Left)"; onTriggered: Mpv.seekRelative(-60) }
                MenuItem { text: "-5m (Ctrl+Shift+Left)"; onTriggered: Mpv.seekRelative(-300) }
            }
            MenuSeparator {}
            MenuItem { text: "Frame step forward (Period)"; onTriggered: Mpv.frameStep(1) }
            MenuItem { text: "Frame step backward (Comma)"; onTriggered: Mpv.frameStep(-1) }
        }

        // 3. Speed
        Menu {
            title: "Speed"
            MenuItem {
                text: "Normal (1.0x)"
                checkable: true
                checked: Math.abs(Mpv.speed - 1.0) < 0.05
                onTriggered: Mpv.resetSpeed()
            }
            MenuItem { text: "Faster +0.1x (C)"; onTriggered: Mpv.adjustSpeed(0.1) }
            MenuItem { text: "Slower -0.1x (X)"; onTriggered: Mpv.adjustSpeed(-0.1) }
            MenuSeparator {}
            MenuItem { text: "0.5x"; onTriggered: Mpv.setSpeed(0.5) }
            MenuItem { text: "0.75x"; onTriggered: Mpv.setSpeed(0.75) }
            MenuItem { text: "1.0x"; onTriggered: Mpv.setSpeed(1.0) }
            MenuItem { text: "1.25x"; onTriggered: Mpv.setSpeed(1.25) }
            MenuItem { text: "1.5x"; onTriggered: Mpv.setSpeed(1.5) }
            MenuItem { text: "2.0x"; onTriggered: Mpv.setSpeed(2.0) }
        }

        // 4. Audio
        Menu {
            title: "Audio"
            Menu {
                title: "Select track"
                Repeater {
                    model: Mpv.tracksOfType("audio")
                    delegate: MenuItem {
                        text: modelData.label
                        checkable: true
                        checked: modelData.selected
                        onTriggered: Mpv.selectTrack(modelData.id)
                    }
                }
            }
            MenuItem {
                text: "Next audio track (A)"
                onTriggered: Mpv.cycleAudioTrack()
            }
            MenuSeparator {}
            MenuItem {
                text: Mpv.muted ? "Unmute (M)" : "Mute (M)"
                onTriggered: {
                    Mpv.setMuted(!Mpv.muted)
                    Mpv.showOsd(Mpv.muted ? "Mute" : "Unmute (" + Mpv.volume + "%)")
                }
            }
            Menu {
                title: "Volume"
                MenuItem { text: "100%"; onTriggered: Mpv.setVolume(100) }
                MenuItem { text: "80%"; onTriggered: Mpv.setVolume(80) }
                MenuItem { text: "50%"; onTriggered: Mpv.setVolume(50) }
                MenuItem { text: "20%"; onTriggered: Mpv.setVolume(20) }
                MenuItem { text: "0%"; onTriggered: Mpv.setVolume(0) }
            }
        }

        // 5. Subtitles
        Menu {
            title: "Subtitles"
            Menu {
                title: "Select track"
                Repeater {
                    model: Mpv.tracksOfType("sub")
                    delegate: MenuItem {
                        text: modelData.label
                        checkable: true
                        checked: modelData.selected
                        onTriggered: Mpv.selectTrack(modelData.id)
                    }
                }
                MenuItem {
                    text: "Off"
                    onTriggered: Mpv.disableSubtitles()
                }
            }
            MenuItem {
                text: "Next subtitle track (S)"
                onTriggered: Mpv.cycleSubtitleTrack()
            }
            MenuItem {
                text: "Visible (V)"
                checkable: true
                checked: Mpv.subtitlesVisible
                onTriggered: {
                    Mpv.setSubtitlesVisible(!Mpv.subtitlesVisible)
                    Mpv.showOsd(Mpv.subtitlesVisible ? "Subtitles: On" : "Subtitles: Off")
                }
            }
            MenuSeparator {}
            Menu {
                title: "Sync / Delay"
                MenuItem {
                    text: "Delay -0.1s ([)"
                    onTriggered: {
                        Mpv.setSubtitleDelay(Mpv.subtitleDelay - 0.1)
                        Mpv.showOsd("Sub delay: " + Mpv.subtitleDelay.toFixed(1) + "s")
                    }
                }
                MenuItem {
                    text: "Delay +0.1s (])"
                    onTriggered: {
                        Mpv.setSubtitleDelay(Mpv.subtitleDelay + 0.1)
                        Mpv.showOsd("Sub delay: " + Mpv.subtitleDelay.toFixed(1) + "s")
                    }
                }
                MenuItem {
                    text: "Reset delay (0.0s)"
                    onTriggered: {
                        Mpv.setSubtitleDelay(0.0)
                        Mpv.showOsd("Sub delay: 0.0s")
                    }
                }
            }
            Menu {
                title: "Font size"
                MenuItem { text: "Increase (Alt+PageUp)"; onTriggered: Mpv.adjustSubtitleFontSize(2) }
                MenuItem { text: "Decrease (Alt+PageDown)"; onTriggered: Mpv.adjustSubtitleFontSize(-2) }
                MenuItem { text: "Reset size (38)"; onTriggered: { Mpv.setSubtitleFontSize(38); Mpv.showOsd("Sub size: 38") } }
            }
            Menu {
                title: "Position"
                MenuItem { text: "Move up (Alt+Up)"; onTriggered: Mpv.adjustSubtitlePosition(-2) }
                MenuItem { text: "Move down (Alt+Down)"; onTriggered: Mpv.adjustSubtitlePosition(2) }
                MenuItem { text: "Reset position (100%)"; onTriggered: { Mpv.setSubtitlePosition(100); Mpv.showOsd("Sub pos: 100%") } }
            }
            MenuSeparator {}
            MenuItem {
                text: "Legacy Persian encoding (CP1256)"
                onTriggered: {
                    Mpv.setSubtitleCodepage("cp1256")
                    Mpv.showOsd("Codepage: CP1256")
                }
            }
            MenuItem {
                text: "UTF-8 encoding"
                onTriggered: {
                    Mpv.setSubtitleCodepage("utf-8")
                    Mpv.showOsd("Codepage: UTF-8")
                }
            }
            MenuItem {
                text: "Open subtitle file…"
                onTriggered: subDialog.open()
            }
        }

        // 6. Video & Aspect Ratio
        Menu {
            title: "Video"
            Menu {
                title: "Aspect ratio"
                MenuItem {
                    text: "Original / Default"
                    onTriggered: Mpv.setVideoAspect("default")
                }
                MenuItem {
                    text: "16:9"
                    onTriggered: Mpv.setVideoAspect("16:9")
                }
                MenuItem {
                    text: "4:3"
                    onTriggered: Mpv.setVideoAspect("4:3")
                }
                MenuItem {
                    text: "2.35:1 (Cinema)"
                    onTriggered: Mpv.setVideoAspect("2.35:1")
                }
            }
        }

        // 7. Window
        Menu {
            title: "Window"
            MenuItem {
                text: "Always on top (Ctrl+T)"
                checkable: true
                checked: root.alwaysOnTop
                onTriggered: root.toggleAlwaysOnTop()
            }
            MenuSeparator {}
            MenuItem { text: "50% size (1)"; onTriggered: { root.visibility = Window.Windowed; root.width = 640; root.height = 360 } }
            MenuItem { text: "100% size (2)"; onTriggered: { root.visibility = Window.Windowed; root.width = 1280; root.height = 720 } }
            MenuItem { text: "150% size (3)"; onTriggered: { root.visibility = Window.Windowed; root.width = 1600; root.height = 900 } }
            MenuItem { text: "200% size (4)"; onTriggered: { root.visibility = Window.Windowed; root.width = 1920; root.height = 1080 } }
            MenuSeparator {}
            MenuItem {
                text: root.visibility === Window.FullScreen ? "Leave fullscreen (F / Enter)" : "Fullscreen (F / Enter)"
                onTriggered: root.toggleFullscreen()
            }
        }

        MenuSeparator {}

        // 8. Exit
        MenuItem {
            text: "Exit (Ctrl+Q)"
            onTriggered: Qt.quit()
        }
    }

    // --- OSD Toast --------------------------------------------------------
    Rectangle {
        id: osdToast
        anchors.top: parent.top
        anchors.topMargin: 28
        anchors.horizontalCenter: parent.horizontalCenter
        width: osdText.implicitWidth + 32
        height: osdText.implicitHeight + 16
        radius: 6
        color: "#d9151515"
        border.color: "#40ffffff"
        border.width: 1
        opacity: 0.0
        visible: opacity > 0.0
        Behavior on opacity { NumberAnimation { duration: 150 } }

        Text {
            id: osdText
            anchors.centerIn: parent
            color: "#ffffff"
            font.pixelSize: 15
            font.bold: true
        }

        Timer {
            id: osdTimer
            interval: 1600
            repeat: false
            onTriggered: osdToast.opacity = 0.0
        }
    }

    Connections {
        target: Mpv
        function onOsdRequested(text) {
            osdText.text = text
            osdToast.opacity = 1.0
            osdTimer.restart()
        }
    }
}
