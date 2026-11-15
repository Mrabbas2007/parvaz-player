import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Parvaz 1.0
import "."
import "components"

Window {
    id: root
    width: 1280
    height: 720
    minimumWidth: 360
    minimumHeight: 200
    visible: true
    color: Theme.bgBase
    title: {
        if (Mpv.isAudioOnly) {
            var art = Mpv.trackArtist;
            var tit = Mpv.trackTitle;
            if (tit.length > 0)
                return (art.length > 0 ? (art + " - ") : "") + tit + " — Parvaz";
        }
        return Mpv.mediaTitle.length > 0 ? Mpv.mediaTitle + " — Parvaz" : "Parvaz Player";
    }

    FontLoader { id: vazirFont; source: "fonts/Vazirmatn.ttf" }
    FontLoader { id: monoMediumFont; source: "fonts/JetBrainsMono-Medium.otf" }
    FontLoader { id: monoRegularFont; source: "fonts/JetBrainsMono-Regular.otf" }

    property bool chromeVisible: true
    property bool pointerOnBar: false
    property bool alwaysOnTop: Config.alwaysOnTop
    flags: Qt.Window | Qt.FramelessWindowHint | (alwaysOnTop ? Qt.WindowStaysOnTopHint : 0)

    function toggleAlwaysOnTop() {
        alwaysOnTop = !alwaysOnTop
        Config.setAlwaysOnTop(alwaysOnTop)
        Mpv.showOsd(alwaysOnTop ? "Always on top: On" : "Always on top: Off")
    }

    function toggleFullscreen() {
        root.visibility = (root.visibility === Window.FullScreen
                           ? Window.Windowed : Window.FullScreen)
    }

    function toggleMaximize() {
        if (root.visibility === Window.Maximized)
            root.showNormal()
        else
            root.showMaximized()
    }

    // Auto-hide: 2.2s of no pointer motion hides chrome while playing.
    Timer {
        id: hideTimer
        interval: Theme.autoHideTimeout
        repeat: false
        onTriggered: {
            if (!Mpv.paused && !root.pointerOnBar && !(titleBar && titleBar.pointerOnBar) && !controlBar.isDraggingSeek && !playlistDrawer.isOpen) {
                root.chromeVisible = false
            }
        }
    }

    function poke() {
        root.chromeVisible = true
        hideTimer.restart()
    }

    onChromeVisibleChanged: {
        if (chromeVisible) {
            poke()
        } else {
            hideTimer.stop()
            pointerOnBar = false
        }
        Mpv.setBottomBarOffset(chromeVisible, controlBar.height, root.height)
    }

    onHeightChanged: {
        Mpv.setBottomBarOffset(chromeVisible, controlBar.height, root.height)
        if (root.visibility === Window.Windowed)
            Config.setWindowSize(root.width, root.height)
    }

    onWidthChanged: {
        if (root.visibility === Window.Windowed)
            Config.setWindowSize(root.width, root.height)
    }

    Connections {
        target: Config
        function onWindowGeometryChanged() {
            if (root.visibility === Window.Windowed) {
                if (root.width !== Config.windowWidth)
                    root.width = Config.windowWidth
                if (root.height !== Config.windowHeight)
                    root.height = Config.windowHeight
            }
        }
        function onAlwaysOnTopChanged() {
            if (root.alwaysOnTop !== Config.alwaysOnTop)
                root.alwaysOnTop = Config.alwaysOnTop
        }
    }

    Connections {
        target: Mpv
        function onPausedChanged() {
            if (!Mpv.paused)
                poke()
        }
    }

    Component.onCompleted: {
        poke()
        if (Config.windowWidth >= root.minimumWidth && Config.windowHeight >= root.minimumHeight) {
            root.width = Config.windowWidth
            root.height = Config.windowHeight
        }
        Mpv.setBottomBarOffset(chromeVisible, controlBar.height, root.height)
    }

    FileDialog {
        id: openDialog
        title: "Open media"
        nameFilters: [
            "Media (*.mkv *.mp4 *.webm *.avi *.mov *.flv *.ts *.ogm *.mp3 *.flac *.ogg *.opus *.wav)",
            "All files (*)"
        ]
        onAccepted: Mpv.loadFile(Mpv.urlToLocalPath(openDialog.selectedFile))
    }

    FileDialog {
        id: addPlaylistDialog
        title: "Add files to playlist"
        fileMode: FileDialog.OpenFiles
        nameFilters: openDialog.nameFilters
        onAccepted: {
            var paths = []
            for (var i = 0; i < selectedFiles.length; ++i) {
                paths.push(Mpv.urlToLocalPath(selectedFiles[i]))
            }
            if (paths.length > 0) {
                Mpv.loadFiles(paths, "append")
                Mpv.showOsd(paths.length === 1 ? "1 file queued" : paths.length + " files queued")
            }
        }
    }

    FileDialog {
        id: subDialog
        title: "Open subtitle file"
        nameFilters: [
            "Subtitles (*.srt *.ass *.ssa *.sub *.vtt *.sup)",
            "All files (*)"
        ]
        onAccepted: Mpv.addSubtitleFile(Mpv.urlToLocalPath(subDialog.selectedFile))
    }

    // --- Video Viewport (libmpv direct FBO) --------------------------------
    MpvFboItem {
        id: video
        anchors.fill: parent
        visible: !Mpv.isAudioOnly
    }

    // --- Dedicated Music Player View (Cover Art / Poster / Vinyl) ----------
    MusicPlayerView {
        id: musicPlayerView
        anchors.fill: parent
    }

    // Mouse interactions over the video area
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        hoverEnabled: true
        onPositionChanged: root.poke()
        onWheel: (wheel) => {
            if (wheel.angleDelta.y > 0) {
                Mpv.adjustVolume(5)
            } else if (wheel.angleDelta.y < 0) {
                Mpv.adjustVolume(-5)
            }
            root.poke()
        }
        onClicked: (mouse) => {
            if (playlistDrawer.isOpen) {
                playlistDrawer.close()
                return
            }
            if (mouse.button === Qt.LeftButton) {
                Mpv.togglePause()
            } else if (mouse.button === Qt.RightButton) {
                contextMenu.popup()
            }
        }
        onDoubleClicked: (mouse) => {
            if (mouse.button === Qt.LeftButton)
                root.toggleFullscreen()
        }
    }

    // Drop file placeholder (shown when idle / no file playing)
    Rectangle {
        visible: Mpv.duration <= 0
        anchors.centerIn: parent
        width: dropText.implicitWidth + 32
        height: 38
        radius: Theme.radiusMedium
        color: "#D90C0D12"
        border.color: Theme.borderGlass
        border.width: 1

        Text {
            id: dropText
            anchors.centerIn: parent
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontNormal
            text: "فایلی را اینجا رها کنید  ·  Space = پخش/توقف  ·  F = تمام‌صفحه"
        }
    }

    // Dual-Zone Drop Target: Play later (top 28%) & Play now (bottom 72%)
    DropOverlay {
        id: dropOverlay
    }

    // --- Top Title Bar -----------------------------------------------------
    TitleBar {
        id: titleBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        mediaTitle: {
            if (Mpv.isAudioOnly) {
                var art = Mpv.trackArtist;
                var tit = Mpv.trackTitle;
                if (tit.length > 0)
                    return (art.length > 0 ? (art + " - ") : "") + tit;
            }
            return Mpv.mediaTitle;
        }
        alwaysOnTop: root.alwaysOnTop
        isFullscreen: root.visibility === Window.FullScreen
        isMaximized: root.visibility === Window.Maximized
        opacity: (root.visibility !== Window.FullScreen || root.chromeVisible) ? 1.0 : 0.0
        visible: opacity > 0.0

        Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }

        onPointerOnBarChanged: {
            if (pointerOnBar)
                hideTimer.stop()
            else if (!Mpv.paused)
                root.poke()
        }

        onPinToggled: root.toggleAlwaysOnTop()
        onFullscreenExitClicked: root.toggleFullscreen()
        onMinimizeClicked: root.showMinimized()
        onMaximizeToggled: root.toggleMaximize()
        onCloseClicked: root.close()
        onStartWindowMove: root.startSystemMove()
    }

    // Source texture for docking blur
    ShaderEffectSource {
        id: dockBlurSource
        sourceItem: Mpv.isAudioOnly ? musicPlayerView : video
        sourceRect: Qt.rect(controlBar.x, controlBar.y, controlBar.width, controlBar.height)
        live: root.chromeVisible
        visible: false
    }

    // --- Grounded Bottom Control Dock --------------------------------------
    ControlBar {
        id: controlBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        isFullscreen: root.visibility === Window.FullScreen
        blurSource: dockBlurSource
        opacity: root.chromeVisible ? 1.0 : 0.0
        visible: opacity > 0.0

        Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }

        onPointerOnBarChanged: {
            root.pointerOnBar = pointerOnBar
            if (pointerOnBar) {
                hideTimer.stop()
            } else if (!Mpv.paused) {
                root.poke()
            }
        }

        playlistOpen: playlistDrawer.isOpen
        onPlaylistToggleRequested: {
            playlistDrawer.toggle()
            root.poke()
        }

        onPokeRequested: root.poke()
        onOpenClicked: openDialog.open()
        onOpenSubClicked: subDialog.open()
        onFullscreenToggled: root.toggleFullscreen()
    }

    // --- PotPlayer Playlist Drawer (F6) -----------------------------------
    PlaylistDrawer {
        id: playlistDrawer
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        z: 20
        onIsOpenChanged: {
            if (isOpen)
                root.poke()
        }
        onAddFilesRequested: addPlaylistDialog.open()
    }

    // --- Non-intrusive Top Corner OSD --------------------------------------
    OsdOverlay {
        id: osdOverlay
    }

    // --- Right-click Context Menu (PotPlayer style) -----------------------
    ContextMenu {
        id: contextMenu
        isAlwaysOnTop: root.alwaysOnTop
        onOpenMediaRequested: openDialog.open()
        onOpenSubtitleRequested: subDialog.open()
        onFullscreenToggled: root.toggleFullscreen()
        onAlwaysOnTopToggled: root.toggleAlwaysOnTop()
        onPlaylistToggled: { playlistDrawer.toggle(); root.poke() }
        onTechnicalInfoRequested: technicalInfoOverlay.toggle()
        onWindowSizeRequested: (w, h) => {
            root.visibility = Window.Windowed
            root.width = w
            root.height = h
        }
    }

    // --- Technical Media Info Overlay (Tab) -------------------------------
    TechnicalInfoOverlay {
        id: technicalInfoOverlay
        anchors.fill: parent
        z: 25
    }

    // --- Keyboard Shortcuts (PotPlayer Standard) ---------------------------
    component PlayerShortcut: Shortcut {
        enabled: !openDialog.visible && !subDialog.visible
    }

    // Playback
    PlayerShortcut { sequence: "Space"; onActivated: Mpv.togglePause() }
    PlayerShortcut { sequence: "Comma"; onActivated: Mpv.frameStep(-1) }
    PlayerShortcut { sequence: "Period"; onActivated: Mpv.frameStep(1) }

    // Seeking (PotPlayer style: Left/Right 5s, Ctrl 30s, Shift 60s, Ctrl+Shift 300s, Backspace/Home 0s)
    PlayerShortcut { sequence: "Left"; onActivated: { Mpv.seekRelative(-5); root.poke() } }
    PlayerShortcut { sequence: "Right"; onActivated: { Mpv.seekRelative(5); root.poke() } }
    PlayerShortcut { sequence: "Ctrl+Left"; onActivated: { Mpv.seekRelative(-30); root.poke() } }
    PlayerShortcut { sequence: "Ctrl+Right"; onActivated: { Mpv.seekRelative(30); root.poke() } }
    PlayerShortcut { sequence: "Shift+Left"; onActivated: { Mpv.seekRelative(-60); root.poke() } }
    PlayerShortcut { sequence: "Shift+Right"; onActivated: { Mpv.seekRelative(60); root.poke() } }
    PlayerShortcut { sequence: "Ctrl+Shift+Left"; onActivated: { Mpv.seekRelative(-300); root.poke() } }
    PlayerShortcut { sequence: "Ctrl+Shift+Right"; onActivated: { Mpv.seekRelative(300); root.poke() } }
    PlayerShortcut { sequence: "Backspace"; onActivated: { Mpv.seekAbsolute(0); Mpv.showOsd("00:00:00"); root.poke() } }
    PlayerShortcut { sequence: "Home"; onActivated: { Mpv.seekAbsolute(0); Mpv.showOsd("00:00:00"); root.poke() } }

    // Speed control (PotPlayer: Z = normal 1.0x, X = -0.1x, C = +0.1x)
    PlayerShortcut { sequence: "Z"; onActivated: Mpv.resetSpeed() }
    PlayerShortcut { sequence: "X"; onActivated: Mpv.adjustSpeed(-0.1) }
    PlayerShortcut { sequence: "C"; onActivated: Mpv.adjustSpeed(0.1) }

    // Volume (PotPlayer: Up/Down arrow = +/- 5%, M = mute)
    PlayerShortcut { sequence: "Up"; onActivated: { Mpv.adjustVolume(5); root.poke() } }
    PlayerShortcut { sequence: "Down"; onActivated: { Mpv.adjustVolume(-5); root.poke() } }
    PlayerShortcut { sequence: "Ctrl+Up"; onActivated: { Mpv.adjustVolume(5); root.poke() } }
    PlayerShortcut { sequence: "Ctrl+Down"; onActivated: { Mpv.adjustVolume(-5); root.poke() } }
    PlayerShortcut {
        sequence: "M"
        onActivated: {
            var newMuted = !Mpv.muted
            Mpv.setMuted(newMuted)
            Mpv.showOsd(newMuted ? "Mute" : "Unmute (" + Mpv.volume + "%)")
            root.poke()
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
    PlayerShortcut {
        sequence: "Escape"
        onActivated: {
            if (technicalInfoOverlay.isOpen) {
                technicalInfoOverlay.close()
            } else if (playlistDrawer.isOpen) {
                playlistDrawer.close()
            } else if (root.visibility === Window.FullScreen) {
                root.visibility = Window.Windowed
            }
        }
    }

    // Technical info overlay (PotPlayer Tab)
    PlayerShortcut { sequence: "Tab"; onActivated: technicalInfoOverlay.toggle() }

    // Bookmarks & Chapters (PotPlayer standard)
    PlayerShortcut { sequence: "P"; onActivated: Mpv.addBookmark() }
    PlayerShortcut { sequence: "Shift+]"; onActivated: Mpv.nextChapter() }
    PlayerShortcut { sequence: "Shift+["; onActivated: Mpv.previousChapter() }

    // Subtitles & Audio tracks
    PlayerShortcut { sequence: "S"; onActivated: Mpv.cycleSubtitleTrack() }
    PlayerShortcut { sequence: "Alt+S"; onActivated: Mpv.cycleSecondarySubtitleTrack() }
    PlayerShortcut { sequence: "A"; onActivated: Mpv.cycleAudioTrack() }
    PlayerShortcut { sequence: "N"; onActivated: Mpv.toggleAudioNormalizer() }
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

    // Playlist shortcuts (PotPlayer Standard: F6 toggle, PgDn/PgUp Next/Prev)
    PlayerShortcut { sequence: "F6"; onActivated: { playlistDrawer.toggle(); root.poke() } }
    PlayerShortcut {
        sequence: "PgDown"
        onActivated: {
            if (Mpv.hasNext) {
                Mpv.next()
                Mpv.showOsd("Next track")
            }
        }
    }
    PlayerShortcut {
        sequence: "PgUp"
        onActivated: {
            if (Mpv.hasPrevious) {
                Mpv.previous()
                Mpv.showOsd("Previous track")
            }
        }
    }

    PlayerShortcut { sequence: "Ctrl+T"; onActivated: root.toggleAlwaysOnTop() }
    PlayerShortcut { sequence: "Ctrl+C"; onActivated: Mpv.copyScreenshotToClipboard() }
    PlayerShortcut { sequence: "Ctrl+S"; onActivated: Mpv.saveScreenshot() }
    PlayerShortcut { sequence: "Ctrl+Q"; onActivated: Qt.quit() }

    // --- Window Border & Edge Resizing (Frameless windowed mode) ------------
    Rectangle {
        id: windowFrameBorder
        anchors.fill: parent
        color: "transparent"
        border.color: Theme.borderGlass
        border.width: 1
        visible: root.visibility === Window.Windowed
        z: 999
    }

    Item {
        id: resizeEdges
        anchors.fill: parent
        visible: root.visibility === Window.Windowed
        z: 998

        MouseArea {
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            anchors.leftMargin: 8; anchors.rightMargin: 8; height: 5
            cursorShape: Qt.SizeVerCursor
            onPressed: root.startSystemResize(Qt.TopEdge)
        }
        MouseArea {
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            anchors.leftMargin: 8; anchors.rightMargin: 8; height: 5
            cursorShape: Qt.SizeVerCursor
            onPressed: root.startSystemResize(Qt.BottomEdge)
        }
        MouseArea {
            anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.left: parent.left
            anchors.topMargin: 8; anchors.bottomMargin: 8; width: 5
            cursorShape: Qt.SizeHorCursor
            onPressed: root.startSystemResize(Qt.LeftEdge)
        }
        MouseArea {
            anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.right: parent.right
            anchors.topMargin: 8; anchors.bottomMargin: 8; width: 5
            cursorShape: Qt.SizeHorCursor
            onPressed: root.startSystemResize(Qt.RightEdge)
        }
        MouseArea {
            anchors.top: parent.top; anchors.left: parent.left; width: 8; height: 8
            cursorShape: Qt.SizeFDiagCursor
            onPressed: root.startSystemResize(Qt.TopEdge | Qt.LeftEdge)
        }
        MouseArea {
            anchors.top: parent.top; anchors.right: parent.right; width: 8; height: 8
            cursorShape: Qt.SizeBDiagCursor
            onPressed: root.startSystemResize(Qt.TopEdge | Qt.RightEdge)
        }
        MouseArea {
            anchors.bottom: parent.bottom; anchors.left: parent.left; width: 8; height: 8
            cursorShape: Qt.SizeBDiagCursor
            onPressed: root.startSystemResize(Qt.BottomEdge | Qt.LeftEdge)
        }
        MouseArea {
            anchors.bottom: parent.bottom; anchors.right: parent.right; width: 8; height: 8
            cursorShape: Qt.SizeFDiagCursor
            onPressed: root.startSystemResize(Qt.BottomEdge | Qt.RightEdge)
        }
    }
}
