import QtQuick
import QtQuick.Controls
import Parvaz 1.0
import ".."

Item {
    id: root
    anchors.fill: parent

    readonly property bool isDragging: topDrop.containsDrag || botDrop.containsDrag
    opacity: isDragging ? 1.0 : 0.0
    visible: opacity > 0.0

    Behavior on opacity {
        NumberAnimation { duration: 120; easing.type: Easing.OutQuad }
    }

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
                ? "۱ فایل به صف اضافه شد"
                : paths.length + " فایل به صف اضافه شد"
            Mpv.showOsd(msg)
        } else {
            Mpv.showOsd("پخش فوری آغاز شد")
        }
    }

    // Translucent dark veil over video (PotPlayer signature look)
    Rectangle {
        anchors.fill: parent
        color: "#80000000"
    }

    // --- Top Section: Play later (~26% height) -----------------------------
    Item {
        id: topSection
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: Math.max(90, Math.round(parent.height * 0.26))

        // PotPlayer wireframe highlight outline
        Rectangle {
            anchors.fill: parent
            anchors.margins: 10
            color: topDrop.containsDrag ? "#1AFFFFFF" : "transparent"
            border.color: topDrop.containsDrag ? "#FFFFFF" : "transparent"
            border.width: 2
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

        DropArea {
            id: topDrop
            anchors.fill: parent
            onDropped: (drop) => root.handleDrop(drop, "append")
        }
    }

    // --- Bottom Section: Play now (~74% height) ----------------------------
    Item {
        id: botSection
        anchors.top: topSection.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        // PotPlayer wireframe highlight outline (active when hovering bottom or entering)
        Rectangle {
            anchors.fill: parent
            anchors.margins: 10
            color: botDrop.containsDrag ? "#1AFFFFFF" : "transparent"
            border.color: (botDrop.containsDrag || !topDrop.containsDrag) ? "#FFFFFF" : "transparent"
            border.width: 2
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

        DropArea {
            id: botDrop
            anchors.fill: parent
            onDropped: (drop) => root.handleDrop(drop, "replace")
        }
    }
}
