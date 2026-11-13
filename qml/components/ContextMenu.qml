import QtQuick
import QtQuick.Controls
import Parvaz 1.0
import ".."

Menu {
    id: root

    signal openMediaRequested()
    signal openSubtitleRequested()
    signal windowSizeRequested(int w, int h)
    signal fullscreenToggled()
    signal alwaysOnTopToggled()

    property bool isAlwaysOnTop: false

    background: Rectangle {
        implicitWidth: 220
        color: Theme.bgGlass
        border.color: Theme.borderGlass
        border.width: 1
        radius: Theme.radiusMedium
    }

    // 1. Open
    Menu {
        title: "Open"
        background: Rectangle {
            implicitWidth: 220
            color: Theme.bgGlass
            border.color: Theme.borderGlass
            border.width: 1
            radius: Theme.radiusMedium
        }
        MenuItem {
            text: "Open media…"
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
            onTriggered: root.openMediaRequested()
        }
        MenuItem {
            text: "Open subtitle file…"
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
            onTriggered: root.openSubtitleRequested()
        }
    }

    MenuSeparator {}

    // 2. Playback
    Menu {
        title: "Playback"
        background: Rectangle {
            implicitWidth: 220
            color: Theme.bgGlass
            border.color: Theme.borderGlass
            border.width: 1
            radius: Theme.radiusMedium
        }
        MenuItem {
            text: Mpv.paused ? "Play" : "Pause"
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
            onTriggered: Mpv.togglePause()
        }
        MenuItem {
            text: "Stop (to beginning)"
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
            onTriggered: { Mpv.seekAbsolute(0); if (!Mpv.paused) Mpv.togglePause() }
        }
        MenuSeparator {}
        Menu {
            title: "Jump forward"
            background: Rectangle {
                implicitWidth: 220
                color: Theme.bgGlass; border.color: Theme.borderGlass; border.width: 1; radius: Theme.radiusMedium
            }
            MenuItem { text: "+5s (Right)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: Mpv.seekRelative(5) }
            MenuItem { text: "+30s (Ctrl+Right)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: Mpv.seekRelative(30) }
            MenuItem { text: "+1m (Shift+Right)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: Mpv.seekRelative(60) }
            MenuItem { text: "+5m (Ctrl+Shift+Right)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: Mpv.seekRelative(300) }
        }
        Menu {
            title: "Jump backward"
            background: Rectangle {
                implicitWidth: 220
                color: Theme.bgGlass; border.color: Theme.borderGlass; border.width: 1; radius: Theme.radiusMedium
            }
            MenuItem { text: "-5s (Left)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: Mpv.seekRelative(-5) }
            MenuItem { text: "-30s (Ctrl+Left)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: Mpv.seekRelative(-30) }
            MenuItem { text: "-1m (Shift+Left)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: Mpv.seekRelative(-60) }
            MenuItem { text: "-5m (Ctrl+Shift+Left)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: Mpv.seekRelative(-300) }
        }
        MenuSeparator {}
        MenuItem { text: "Frame step forward (Period)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: Mpv.frameStep(1) }
        MenuItem { text: "Frame step backward (Comma)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: Mpv.frameStep(-1) }
    }

    // 3. Speed
    Menu {
        title: "Speed"
        background: Rectangle {
            implicitWidth: 220
            color: Theme.bgGlass; border.color: Theme.borderGlass; border.width: 1; radius: Theme.radiusMedium
        }
        MenuItem {
            text: "Normal (1.0x)"
            checkable: true
            checked: Math.abs(Mpv.speed - 1.0) < 0.05
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
            onTriggered: Mpv.resetSpeed()
        }
        MenuItem { text: "Faster +0.1x (C)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: Mpv.adjustSpeed(0.1) }
        MenuItem { text: "Slower -0.1x (X)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: Mpv.adjustSpeed(-0.1) }
        MenuSeparator {}
        MenuItem { text: "0.5x"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: Mpv.setSpeed(0.5) }
        MenuItem { text: "0.75x"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: Mpv.setSpeed(0.75) }
        MenuItem { text: "1.0x"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: Mpv.setSpeed(1.0) }
        MenuItem { text: "1.25x"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: Mpv.setSpeed(1.25) }
        MenuItem { text: "1.5x"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: Mpv.setSpeed(1.5) }
        MenuItem { text: "2.0x"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: Mpv.setSpeed(2.0) }
    }

    // 4. Audio
    Menu {
        title: "Audio"
        background: Rectangle {
            implicitWidth: 220
            color: Theme.bgGlass; border.color: Theme.borderGlass; border.width: 1; radius: Theme.radiusMedium
        }
        Menu {
            title: "Select track"
            background: Rectangle {
                implicitWidth: 220
                color: Theme.bgGlass; border.color: Theme.borderGlass; border.width: 1; radius: Theme.radiusMedium
            }
            Repeater {
                model: Mpv.tracksOfType("audio")
                delegate: MenuItem {
                    text: modelData.label
                    checkable: true
                    checked: modelData.selected
                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
                    onTriggered: Mpv.selectTrack(modelData.id)
                }
            }
        }
        MenuItem { text: "Cycle audio track (A)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: Mpv.cycleAudioTrack() }
        MenuSeparator {}
        MenuItem {
            text: Mpv.muted ? "Unmute (M)" : "Mute (M)"
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
            onTriggered: {
                Mpv.setMuted(!Mpv.muted)
                Mpv.showOsd(Mpv.muted ? "Mute" : "Unmute (" + Mpv.volume + "%)")
            }
        }
        MenuItem {
            text: "Volume +5% (Up)"
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
            onTriggered: Mpv.adjustVolume(5)
        }
        MenuItem {
            text: "Volume -5% (Down)"
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
            onTriggered: Mpv.adjustVolume(-5)
        }
    }

    // 5. Subtitles
    Menu {
        title: "Subtitles"
        background: Rectangle {
            implicitWidth: 240
            color: Theme.bgGlass; border.color: Theme.borderGlass; border.width: 1; radius: Theme.radiusMedium
        }
        Menu {
            title: "Select track"
            background: Rectangle {
                implicitWidth: 220
                color: Theme.bgGlass; border.color: Theme.borderGlass; border.width: 1; radius: Theme.radiusMedium
            }
            Repeater {
                model: Mpv.tracksOfType("sub")
                delegate: MenuItem {
                    text: modelData.label
                    checkable: true
                    checked: modelData.selected
                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
                    onTriggered: Mpv.selectTrack(modelData.id)
                }
            }
        }
        MenuItem { text: "Disable subtitles"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: Mpv.disableSubtitles() }
        MenuItem { text: "Cycle subtitle track (S)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: Mpv.cycleSubtitleTrack() }
        MenuSeparator {}
        MenuItem {
            text: "Subtitle delay +0.1s (])"
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
            onTriggered: {
                Mpv.setSubtitleDelay(Mpv.subtitleDelay + 0.1)
                Mpv.showOsd("Sub delay: " + Mpv.subtitleDelay.toFixed(1) + "s")
            }
        }
        MenuItem {
            text: "Subtitle delay -0.1s ([)"
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
            onTriggered: {
                Mpv.setSubtitleDelay(Mpv.subtitleDelay - 0.1)
                Mpv.showOsd("Sub delay: " + Mpv.subtitleDelay.toFixed(1) + "s")
            }
        }
        MenuItem {
            text: "Subtitle font size + (Alt+PageUp)"
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
            onTriggered: Mpv.adjustSubtitleFontSize(2)
        }
        MenuItem {
            text: "Subtitle font size - (Alt+PageDown)"
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
            onTriggered: Mpv.adjustSubtitleFontSize(-2)
        }
        MenuItem {
            text: "Move subtitle up (Alt+Up)"
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
            onTriggered: Mpv.adjustSubtitlePosition(-2)
        }
        MenuItem {
            text: "Move subtitle down (Alt+Down)"
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
            onTriggered: Mpv.adjustSubtitlePosition(2)
        }
        MenuItem {
            text: "Show subtitles (V)"
            checkable: true
            checked: Mpv.subtitlesVisible
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
            onTriggered: {
                Mpv.setSubtitlesVisible(!Mpv.subtitlesVisible)
                Mpv.showOsd(Mpv.subtitlesVisible ? "Subtitles: On" : "Subtitles: Off")
            }
        }
        MenuSeparator {}
        MenuItem {
            text: "Persian legacy encoding (cp1256)"
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
            onTriggered: Mpv.setSubtitleCodepage("cp1256")
        }
        MenuItem {
            text: "UTF-8 encoding (default)"
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
            onTriggered: Mpv.setSubtitleCodepage("utf-8")
        }
        MenuItem {
            text: "Open subtitle file…"
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
            onTriggered: root.openSubtitleRequested()
        }
    }

    // 6. Video (Aspect ratio)
    Menu {
        title: "Video"
        background: Rectangle {
            implicitWidth: 220
            color: Theme.bgGlass; border.color: Theme.borderGlass; border.width: 1; radius: Theme.radiusMedium
        }
        Menu {
            title: "Aspect ratio"
            background: Rectangle {
                implicitWidth: 220
                color: Theme.bgGlass; border.color: Theme.borderGlass; border.width: 1; radius: Theme.radiusMedium
            }
            MenuItem {
                text: "Original (Default)"
                checkable: true
                checked: Mpv.videoAspect === "default"
                font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
                onTriggered: Mpv.setVideoAspect("default")
            }
            MenuItem {
                text: "16:9"
                checkable: true
                checked: Mpv.videoAspect === "16:9"
                font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
                onTriggered: Mpv.setVideoAspect("16:9")
            }
            MenuItem {
                text: "4:3"
                checkable: true
                checked: Mpv.videoAspect === "4:3"
                font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
                onTriggered: Mpv.setVideoAspect("4:3")
            }
            MenuItem {
                text: "2.35:1 (Cinemascope)"
                checkable: true
                checked: Mpv.videoAspect === "2.35:1"
                font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
                onTriggered: Mpv.setVideoAspect("2.35:1")
            }
        }
    }

    // 7. Window
    Menu {
        title: "Window"
        background: Rectangle {
            implicitWidth: 220
            color: Theme.bgGlass; border.color: Theme.borderGlass; border.width: 1; radius: Theme.radiusMedium
        }
        MenuItem {
            text: "Always on top (Ctrl+T)"
            checkable: true
            checked: root.isAlwaysOnTop
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
            onTriggered: root.alwaysOnTopToggled()
        }
        MenuSeparator {}
        MenuItem { text: "50% size (1)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: root.windowSizeRequested(640, 360) }
        MenuItem { text: "100% size (2)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: root.windowSizeRequested(1280, 720) }
        MenuItem { text: "150% size (3)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: root.windowSizeRequested(1600, 900) }
        MenuItem { text: "200% size (4)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; onTriggered: root.windowSizeRequested(1920, 1080) }
        MenuSeparator {}
        MenuItem {
            text: "Toggle fullscreen (F / Enter)"
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
            onTriggered: root.fullscreenToggled()
        }
    }

    MenuSeparator {}

    // 8. Exit
    MenuItem {
        text: "Exit (Ctrl+Q)"
        font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
        onTriggered: Qt.quit()
    }
}
