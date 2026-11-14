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
        NumberAnimation { duration: 160; easing.type: Easing.OutQuad }
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
                ? "۱ فایل به انتهای صف اضافه شد"
                : paths.length + " فایل به صف اضافه شد"
            Mpv.showOsd(msg)
        } else {
            Mpv.showOsd("پخش فوری آغاز شد")
        }
    }

    // Semi-translucent dark veil
    Rectangle {
        anchors.fill: parent
        color: "#D90B0D13"
    }

    // --- Top Zone: Play Later (~28% height) ---------------------------------
    Item {
        id: topSection
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: Math.max(120, Math.round(parent.height * 0.28))

        Rectangle {
            anchors.fill: parent
            anchors.margins: 14
            radius: Theme.radiusMedium
            color: topDrop.containsDrag ? "#252B3B" : "#12151E"
            border.color: topDrop.containsDrag ? "#FFFFFF" : Theme.borderGlass
            border.width: topDrop.containsDrag ? 2 : 1

            Behavior on color { ColorAnimation { duration: 120 } }
            Behavior on border.color { ColorAnimation { duration: 120 } }

            Column {
                anchors.centerIn: parent
                spacing: 6

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 10

                    Image {
                        width: 22
                        height: 22
                        source: "../icons/list-plus.svg"
                        sourceSize: Qt.size(22, 22)
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: "Play later"
                        color: Theme.textPrimary
                        font.family: Theme.fontDigits
                        font.pixelSize: 22
                        font.weight: Font.DemiBold
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "افزودن به انتهای صف پخش (بدون وقفه در ترک فعلی)"
                    color: topDrop.containsDrag ? Theme.textPrimary : Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                }
            }
        }

        DropArea {
            id: topDrop
            anchors.fill: parent
            onDropped: (drop) => root.handleDrop(drop, "append")
        }
    }

    // --- Bottom Zone: Play Now (~72% height) --------------------------------
    Item {
        id: botSection
        anchors.top: topSection.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        Rectangle {
            anchors.fill: parent
            anchors.margins: 14
            radius: Theme.radiusMedium
            color: botDrop.containsDrag ? "#252B3B" : "#12151E"
            border.color: botDrop.containsDrag ? "#FFFFFF" : Theme.borderGlass
            border.width: botDrop.containsDrag ? 2 : 1

            Behavior on color { ColorAnimation { duration: 120 } }
            Behavior on border.color { ColorAnimation { duration: 120 } }

            Column {
                anchors.centerIn: parent
                spacing: 10

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 14

                    Image {
                        width: 32
                        height: 32
                        source: "../icons/play.svg"
                        sourceSize: Qt.size(32, 32)
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: "Play now"
                        color: Theme.textPrimary
                        font.family: Theme.fontDigits
                        font.pixelSize: 36
                        font.weight: Font.Bold
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "جایگزینی و شروع پخش بلافاصله"
                    color: botDrop.containsDrag ? Theme.textPrimary : Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontNormal
                }
            }
        }

        DropArea {
            id: botDrop
            anchors.fill: parent
            onDropped: (drop) => root.handleDrop(drop, "replace")
        }
    }
}
