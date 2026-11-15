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
    signal playlistToggled()
    signal technicalInfoRequested()

    property bool isAlwaysOnTop: false

    delegate: StyledItem {}

    background: Rectangle {
        implicitWidth: 230
        color: "#14161F"
        border.color: "#282D3E"
        border.width: 1
        radius: 4
    }

    component StyledItem: MenuItem {
        id: item
        implicitWidth: 230
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
                visible: item.checkable

                Image {
                    anchors.centerIn: parent
                    width: 11
                    height: 11
                    source: "../icons/check.svg"
                    sourceSize: Qt.size(11, 11)
                    visible: item.checked
                }
            }

            Text {
                anchors.left: item.checkable ? chkContainer.right : parent.left
                anchors.leftMargin: item.checkable ? 6 : 6
                anchors.right: arrowIcon.visible ? arrowIcon.left : parent.right
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                text: item.text
                color: !item.enabled ? "#555A6B" : (item.highlighted ? "#FFFFFF" : "#E2E4EC")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontNormal
                elide: Text.ElideRight
            }

            Image {
                id: arrowIcon
                anchors.right: parent.right
                anchors.rightMargin: 2
                anchors.verticalCenter: parent.verticalCenter
                width: 10
                height: 10
                source: "../icons/chevron-right.svg"
                sourceSize: Qt.size(10, 10)
                visible: item.subMenu !== null && item.subMenu !== undefined
            }
        }

        background: Rectangle {
            implicitWidth: 230
            implicitHeight: 28
            color: item.highlighted ? "#2A3348" : "transparent"
            radius: 3
        }
    }

    component StyledMenu: Menu {
        id: subMenu
        delegate: StyledItem {}
        background: Rectangle {
            implicitWidth: 230
            color: "#14161F"
            border.color: "#282D3E"
            border.width: 1
            radius: 4
        }
    }

    component StyledSeparator: MenuSeparator {
        padding: 4
        topPadding: 4
        bottomPadding: 4
        contentItem: Rectangle {
            implicitHeight: 1
            implicitWidth: 216
            color: "#222736"
        }
    }

    // 1. Open
    StyledMenu {
        title: "Open"
        StyledItem {
            text: "Open media…"
            onTriggered: root.openMediaRequested()
        }
        StyledItem {
            text: "Open subtitle file…"
            onTriggered: root.openSubtitleRequested()
        }
    }

    StyledSeparator {}

    // 2. Playback
    StyledMenu {
        title: "Playback"
        StyledItem {
            text: Mpv.paused ? "Play" : "Pause"
            onTriggered: Mpv.togglePause()
        }
        StyledItem {
            text: "Stop (to beginning)"
            onTriggered: { Mpv.seekAbsolute(0); if (!Mpv.paused) Mpv.togglePause() }
        }
        StyledSeparator {}
        StyledMenu {
            title: "Jump forward"
            StyledItem { text: "+5s (Right)"; onTriggered: Mpv.seekRelative(5) }
            StyledItem { text: "+30s (Ctrl+Right)"; onTriggered: Mpv.seekRelative(30) }
            StyledItem { text: "+1m (Shift+Right)"; onTriggered: Mpv.seekRelative(60) }
            StyledItem { text: "+5m (Ctrl+Shift+Right)"; onTriggered: Mpv.seekRelative(300) }
        }
        StyledMenu {
            title: "Jump backward"
            StyledItem { text: "-5s (Left)"; onTriggered: Mpv.seekRelative(-5) }
            StyledItem { text: "-30s (Ctrl+Left)"; onTriggered: Mpv.seekRelative(-30) }
            StyledItem { text: "-1m (Shift+Left)"; onTriggered: Mpv.seekRelative(-60) }
            StyledItem { text: "-5m (Ctrl+Shift+Left)"; onTriggered: Mpv.seekRelative(-300) }
        }
        StyledSeparator {}
        StyledItem { text: "Frame step forward (Period)"; onTriggered: Mpv.frameStep(1) }
        StyledItem { text: "Frame step backward (Comma)"; onTriggered: Mpv.frameStep(-1) }
    }

    // 3. Speed
    StyledMenu {
        title: "Speed"
        StyledItem {
            text: "Normal (1.0x)"
            checkable: true
            checked: Math.abs(Mpv.speed - 1.0) < 0.05
            onTriggered: Mpv.resetSpeed()
        }
        StyledItem { text: "Faster +0.1x (C)"; onTriggered: Mpv.adjustSpeed(0.1) }
        StyledItem { text: "Slower -0.1x (X)"; onTriggered: Mpv.adjustSpeed(-0.1) }
        StyledSeparator {}
        StyledItem { text: "0.5x"; onTriggered: Mpv.setSpeed(0.5) }
        StyledItem { text: "0.75x"; onTriggered: Mpv.setSpeed(0.75) }
        StyledItem { text: "1.0x"; onTriggered: Mpv.setSpeed(1.0) }
        StyledItem { text: "1.25x"; onTriggered: Mpv.setSpeed(1.25) }
        StyledItem { text: "1.5x"; onTriggered: Mpv.setSpeed(1.5) }
        StyledItem { text: "2.0x"; onTriggered: Mpv.setSpeed(2.0) }
    }

    // 4. Audio
    StyledMenu {
        title: "Audio"
        StyledMenu {
            title: "Select track"
            Repeater {
                model: Mpv.audioTracks
                delegate: StyledItem {
                    text: modelData.label
                    checkable: true
                    checked: modelData.selected
                    onTriggered: Mpv.selectAudioTrack(modelData.id)
                }
            }
        }
        StyledItem { text: "Cycle audio track (A)"; onTriggered: Mpv.cycleAudioTrack() }
        StyledSeparator {}
        StyledItem {
            text: "Night mode (Normalizer) (N)"
            checkable: true
            checked: Mpv.audioNormalizer
            onTriggered: Mpv.toggleAudioNormalizer()
        }
        StyledSeparator {}
        StyledItem {
            text: Mpv.muted ? "Unmute (M)" : "Mute (M)"
            onTriggered: {
                var newMuted = !Mpv.muted
                Mpv.setMuted(newMuted)
                Mpv.showOsd(newMuted ? "Mute" : "Unmute (" + Mpv.volume + "%)")
            }
        }
        StyledItem {
            text: "Volume +5% (Up)"
            onTriggered: Mpv.adjustVolume(5)
        }
        StyledItem {
            text: "Volume -5% (Down)"
            onTriggered: Mpv.adjustVolume(-5)
        }
    }

    // 5. Subtitles
    StyledMenu {
        title: "Subtitles"
        StyledMenu {
            title: "Select track"
            StyledItem {
                text: "None"
                checkable: true
                checked: Mpv.currentSubtitleTrack <= 0
                onTriggered: Mpv.selectSubtitleTrack(0)
            }
            StyledSeparator { visible: Mpv.subTracks.length > 0 }
            Repeater {
                model: Mpv.subTracks
                delegate: StyledItem {
                    text: modelData.label
                    checkable: true
                    checked: modelData.selected
                    onTriggered: Mpv.selectSubtitleTrack(modelData.id)
                }
            }
        }
        StyledItem {
            text: Mpv.subtitlesVisible ? "Disable subtitles" : "Enable subtitles"
            onTriggered: {
                var newVisible = !Mpv.subtitlesVisible
                Mpv.setSubtitlesVisible(newVisible)
                Mpv.showOsd(newVisible ? "Subtitles: On" : "Subtitles: Off")
            }
        }
        StyledItem { text: "Cycle subtitle track (S)"; onTriggered: Mpv.cycleSubtitleTrack() }
        StyledSeparator {}
        StyledMenu {
            title: "Secondary subtitle (Top)"
            StyledItem {
                text: "None"
                checkable: true
                checked: Mpv.secondarySubtitleTrack <= 0
                onTriggered: Mpv.selectSecondarySubtitleTrack(0)
            }
            StyledSeparator {}
            Repeater {
                model: Mpv.subTracks
                delegate: StyledItem {
                    text: modelData.label
                    checkable: true
                    checked: modelData.id === Mpv.secondarySubtitleTrack
                    onTriggered: Mpv.selectSecondarySubtitleTrack(modelData.id)
                }
            }
        }
        StyledItem { text: "Cycle secondary subtitle (Alt+S)"; onTriggered: Mpv.cycleSecondarySubtitleTrack() }
        StyledSeparator {}
        StyledItem {
            text: "Subtitle delay +0.1s (])"
            onTriggered: {
                Mpv.setSubtitleDelay(Mpv.subtitleDelay + 0.1)
                Mpv.showOsd("Sub delay: " + Mpv.subtitleDelay.toFixed(1) + "s")
            }
        }
        StyledItem {
            text: "Subtitle delay -0.1s ([)"
            onTriggered: {
                Mpv.setSubtitleDelay(Mpv.subtitleDelay - 0.1)
                Mpv.showOsd("Sub delay: " + Mpv.subtitleDelay.toFixed(1) + "s")
            }
        }
        StyledItem {
            text: "Subtitle font size + (Alt+PageUp)"
            onTriggered: Mpv.adjustSubtitleFontSize(2)
        }
        StyledItem {
            text: "Subtitle font size - (Alt+PageDown)"
            onTriggered: Mpv.adjustSubtitleFontSize(-2)
        }
        StyledItem {
            text: "Move subtitle up (Alt+Up)"
            onTriggered: Mpv.adjustSubtitlePosition(-2)
        }
        StyledItem {
            text: "Move subtitle down (Alt+Down)"
            onTriggered: Mpv.adjustSubtitlePosition(2)
        }
        StyledItem {
            text: "Show subtitles (V)"
            checkable: true
            checked: Mpv.subtitlesVisible
            onTriggered: {
                Mpv.setSubtitlesVisible(!Mpv.subtitlesVisible)
                Mpv.showOsd(Mpv.subtitlesVisible ? "Subtitles: On" : "Subtitles: Off")
            }
        }
        StyledSeparator {}
        StyledItem {
            text: "Persian legacy encoding (cp1256)"
            onTriggered: Mpv.setSubtitleCodepage("cp1256")
        }
        StyledItem {
            text: "UTF-8 encoding (default)"
            onTriggered: Mpv.setSubtitleCodepage("utf-8")
        }
        StyledItem {
            text: "Open subtitle file…"
            onTriggered: root.openSubtitleRequested()
        }
    }

    // 6. Video (Aspect ratio)
    StyledMenu {
        title: "Video"
        StyledMenu {
            title: "Aspect ratio"
            StyledItem {
                text: "Original (Default)"
                checkable: true
                checked: Mpv.videoAspect === "default"
                onTriggered: Mpv.setVideoAspect("default")
            }
            StyledItem {
                text: "16:9"
                checkable: true
                checked: Mpv.videoAspect === "16:9"
                onTriggered: Mpv.setVideoAspect("16:9")
            }
            StyledItem {
                text: "4:3"
                checkable: true
                checked: Mpv.videoAspect === "4:3"
                onTriggered: Mpv.setVideoAspect("4:3")
            }
            StyledItem {
                text: "2.35:1 (Cinemascope)"
                checkable: true
                checked: Mpv.videoAspect === "2.35:1"
                onTriggered: Mpv.setVideoAspect("2.35:1")
            }
        }
    }

    // 7. Bookmarks & Chapters
    StyledMenu {
        title: "Bookmarks & Chapters"
        StyledItem {
            text: "Add bookmark (P)"
            onTriggered: Mpv.addBookmark()
        }
        StyledItem {
            text: "Clear all bookmarks"
            enabled: Mpv.bookmarks.length > 0
            onTriggered: Mpv.clearBookmarks()
        }
        StyledSeparator {}
        StyledItem {
            text: "Next chapter (Shift+])"
            enabled: Mpv.chapters.length > 0
            onTriggered: Mpv.nextChapter()
        }
        StyledItem {
            text: "Previous chapter (Shift+[)"
            enabled: Mpv.chapters.length > 0
            onTriggered: Mpv.previousChapter()
        }
        StyledSeparator {}
        StyledMenu {
            title: "Chapters (" + Mpv.chapters.length + ")"
            enabled: Mpv.chapters.length > 0
            Repeater {
                model: Mpv.chapters
                delegate: StyledItem {
                    text: modelData.title + " (" + Mpv.formatTime(modelData.time) + ")"
                    checkable: true
                    checked: Mpv.currentChapter === modelData.index
                    onTriggered: Mpv.setChapter(modelData.index)
                }
            }
        }
        StyledMenu {
            title: "Bookmarks (" + Mpv.bookmarks.length + ")"
            enabled: Mpv.bookmarks.length > 0
            Repeater {
                model: Mpv.bookmarks
                delegate: StyledItem {
                    text: "★ " + modelData.title + " (" + Mpv.formatTime(modelData.position) + ")"
                    onTriggered: Mpv.seekAbsolute(modelData.position)
                }
            }
        }
    }

    // 8. Playlist
    StyledItem {
        text: "Playlist (F6)"
        onTriggered: root.playlistToggled()
    }

    // 9. Performance Profile
    StyledMenu {
        title: "Performance Profile"
        StyledItem {
            text: "Auto (Adapt to hardware)"
            checkable: true
            checked: Mpv.performanceProfile === "auto"
            onTriggered: Mpv.setPerformanceProfile("auto")
        }
        StyledItem {
            text: "Low-Spec / Eco (Bilinear & Low RAM)"
            checkable: true
            checked: Mpv.performanceProfile === "low-spec"
            onTriggered: Mpv.setPerformanceProfile("low-spec")
        }
        StyledItem {
            text: "High Quality (Spline36 & Deband)"
            checkable: true
            checked: Mpv.performanceProfile === "high-quality"
            onTriggered: Mpv.setPerformanceProfile("high-quality")
        }
    }

    // 10. Window
    StyledMenu {
        title: "Window"
        StyledItem {
            text: "Always on top (Ctrl+T)"
            checkable: true
            checked: root.isAlwaysOnTop
            onTriggered: root.alwaysOnTopToggled()
        }
        StyledItem {
            text: "Technical info overlay (Tab)"
            onTriggered: root.technicalInfoRequested()
        }
        StyledSeparator {}
        StyledItem { text: "50% size (1)"; onTriggered: root.windowSizeRequested(640, 360) }
        StyledItem { text: "100% size (2)"; onTriggered: root.windowSizeRequested(1280, 720) }
        StyledItem { text: "150% size (3)"; onTriggered: root.windowSizeRequested(1600, 900) }
        StyledItem { text: "200% size (4)"; onTriggered: root.windowSizeRequested(1920, 1080) }
        StyledSeparator {}
        StyledItem {
            text: "Toggle fullscreen (F / Enter)"
            onTriggered: root.fullscreenToggled()
        }
    }

    StyledSeparator {}

    // 9. Exit
    StyledItem {
        text: "Exit (Ctrl+Q)"
        onTriggered: Qt.quit()
    }
}
