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

    Component.onCompleted: poke()

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
            if (mouse.button === Qt.LeftButton)
                Mpv.togglePause()
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
        enabled: !openDialog.visible
    }

    PlayerShortcut { sequence: "Space";  onActivated: Mpv.togglePause() }
    PlayerShortcut { sequence: "Left";   onActivated: Mpv.seekRelative(-5) }
    PlayerShortcut { sequence: "Right";  onActivated: Mpv.seekRelative(5) }
    PlayerShortcut { sequence: "Down";   onActivated: Mpv.seekRelative(-30) }
    PlayerShortcut { sequence: "Up";     onActivated: Mpv.seekRelative(30) }
    PlayerShortcut { sequence: "F";      onActivated: root.toggleFullscreen() }
    PlayerShortcut { sequence: "Escape"; onActivated: if (root.visibility === Window.FullScreen) root.visibility = Window.Windowed }

    // Volume / mute / frame step. These keys are free: Space, the arrows,
    // F, Esc and Ctrl+Q are taken above.
    PlayerShortcut { sequence: "M"; onActivated: Mpv.setMuted(!Mpv.muted) }
    PlayerShortcut {
        sequence: "Ctrl+Up"
        onActivated: {
            Mpv.setVolume(Math.min(100, Mpv.volume + 5))
            poke()
        }
    }
    PlayerShortcut {
        sequence: "Ctrl+Down"
        onActivated: {
            Mpv.setVolume(Math.max(0, Mpv.volume - 5))
            poke()
        }
    }
    PlayerShortcut { sequence: "Comma";  onActivated: Mpv.frameStep(-1) }
    PlayerShortcut { sequence: "Period"; onActivated: Mpv.frameStep(1) }

    PlayerShortcut { sequence: "Ctrl+Q"; onActivated: Qt.quit() }
}
