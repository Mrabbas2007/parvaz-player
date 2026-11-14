import QtQuick
import QtQuick.Controls
import Parvaz 1.0
import ".."

Item {
    id: root
    anchors.fill: parent

    // DropAreas must remain visible at all times so Wayland delivers drag events.
    readonly property bool isDragging: topDrop.containsDrag || botDrop.containsDrag
    readonly property real topHeight: Math.max(90, Math.round(parent.height * 0.26))

    function extractPaths(urls) {
        var paths = []
        for (var i = 0; i < urls.length; ++i) {
            var s = urls[i].toString()
            if (s.startsWith("file://"))
                s = s.substring(7)
            paths.push(decodeURIComponent(s))
        }
        return paths
    }

    function handleDrop(drop, mode) {
        if (!drop.hasUrls || drop.urls.length === 0)
            return
        var paths = extractPaths(drop.urls)
        if (paths.length === 0)
            return

        Mpv.loadFiles(paths, mode)

        if (mode === "append") {
            var msg = paths.length === 1
                ? "1 file queued to playlist"
                : paths.length + " files queued to playlist"
            Mpv.showOsd(msg)
        } else {
            Mpv.showOsd("Playing: " + (paths.length > 0 ? paths[0].split('/').pop() : ""))
        }
    }

    // Visual overlay container: only visible while dragging over the window
    Item {
        id: visualOverlay
        anchors.fill: parent
        opacity: root.isDragging ? 1.0 : 0.0
        visible: opacity > 0.0

        Behavior on opacity {
            NumberAnimation { duration: 140; easing.type: Easing.OutQuad }
        }

        // Translucent dark veil over video (PotPlayer signature look)
        Rectangle {
            anchors.fill: parent
            color: "#8A000000"
        }

        // --- Top Section Visual: Play later (~26% height) ---------------------
        Item {
            id: topVisual
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: root.topHeight

            Rectangle {
                anchors.fill: parent
                anchors.margins: 10
                color: topDrop.containsDrag ? "#22FFFFFF" : "transparent"
                border.color: topDrop.containsDrag ? "#FFFFFF" : "#50FFFFFF"
                border.width: topDrop.containsDrag ? 2 : 1
                radius: 4
            }

            Text {
                anchors.centerIn: parent
                text: "Play later"
                color: "#FFFFFF"
                font.family: Theme.fontFamily
                font.pixelSize: Math.round(Math.min(parent.height * 0.40, 32))
                font.weight: Font.DemiBold
                style: Text.Outline
                styleColor: "#80000000"
            }
        }

        // --- Bottom Section Visual: Play now (~74% height) --------------------
        Item {
            anchors.top: topVisual.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom

            Rectangle {
                anchors.fill: parent
                anchors.margins: 10
                color: botDrop.containsDrag ? "#22FFFFFF" : "transparent"
                border.color: (botDrop.containsDrag || !topDrop.containsDrag) ? "#FFFFFF" : "#50FFFFFF"
                border.width: (botDrop.containsDrag || !topDrop.containsDrag) ? 2 : 1
                radius: 4
            }

            Text {
                anchors.centerIn: parent
                text: "Play now"
                color: "#FFFFFF"
                font.family: Theme.fontFamily
                font.pixelSize: Math.round(Math.min(parent.height * 0.24, 52))
                font.weight: Font.DemiBold
                style: Text.Outline
                styleColor: "#80000000"
            }
        }
    }

    // --- Active Drop Targets (Always active in scene graph to accept drags) ---
    Item {
        id: topSection
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: root.topHeight

        DropArea {
            id: topDrop
            anchors.fill: parent
            onEntered: (drag) => drag.acceptProposedAction()
            onPositionChanged: (drag) => drag.acceptProposedAction()
            onDropped: (drop) => root.handleDrop(drop, "append")
        }
    }

    Item {
        id: botSection
        anchors.top: topSection.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        DropArea {
            id: botDrop
            anchors.fill: parent
            onEntered: (drag) => drag.acceptProposedAction()
            onPositionChanged: (drag) => drag.acceptProposedAction()
            onDropped: (drop) => root.handleDrop(drop, "replace")
        }
    }
}
