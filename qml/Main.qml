import QtQuick
import QtQuick.Controls
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

    function localPath(u) {
        var s = u.toString()
        if (s.startsWith("file://"))
            s = s.substring(7)
        return decodeURIComponent(s)
    }

    MpvFboItem {
        id: video
        anchors.fill: parent
    }

    // --- temporary spike HUD (replaced by ControlBar in phase 1) ---
    Rectangle {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 16
        radius: 8
        color: "#99000000"
        width: hud.implicitWidth + 24
        height: hud.implicitHeight + 14

        Row {
            id: hud
            anchors.centerIn: parent
            spacing: 14

            Text {
                color: "#f0f0f0"
                font.pixelSize: 15
                text: Mpv.formatTime(Mpv.position) + " / " + Mpv.formatTime(Mpv.duration)
            }
            Text {
                color: Mpv.hwdec.length > 0 ? "#8fd18f" : "#d18f8f"
                font.pixelSize: 13
                text: "hw: " + (Mpv.hwdec.length > 0 ? Mpv.hwdec : "none")
            }
            Text {
                color: "#f0f0f0"
                font.pixelSize: 13
                text: Mpv.paused ? "PAUSED" : ""
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

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        onClicked: (mouse) => {
            if (mouse.button === Qt.LeftButton)
                Mpv.togglePause()
        }
    }

    DropArea {
        anchors.fill: parent
        onDropped: (drop) => {
            if (drop.hasUrls && drop.urls.length > 0)
                Mpv.loadFile(root.localPath(drop.urls[0]))
        }
    }

    Shortcut { sequence: "Space";  onActivated: Mpv.togglePause() }
    Shortcut { sequence: "Left";   onActivated: Mpv.seekRelative(-5) }
    Shortcut { sequence: "Right";  onActivated: Mpv.seekRelative(5) }
    Shortcut { sequence: "Down";   onActivated: Mpv.seekRelative(-30) }
    Shortcut { sequence: "Up";     onActivated: Mpv.seekRelative(30) }
    Shortcut {
        sequence: "F"
        onActivated: root.visibility =
            (root.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen)
    }
    Shortcut {
        sequence: "Escape"
        onActivated: if (root.visibility === Window.FullScreen) root.visibility = Window.Windowed
    }
    Shortcut { sequence: "Ctrl+Q"; onActivated: Qt.quit() }
}
