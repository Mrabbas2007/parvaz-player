import QtQuick
import QtQuick.Controls
import Parvaz 1.0
import ".."

Item {
    id: root
    width: Math.min(380, parent.width * 0.85)
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    anchors.right: parent.right

    property bool isOpen: false
    function toggle() { isOpen = !isOpen }
    function open() { isOpen = true }
    function close() { isOpen = false }

    // Slide animation from right edge
    x: isOpen ? parent.width - width : parent.width
    visible: x < parent.width

    Behavior on x {
        NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
    }

    property string searchQuery: ""

    // Background frosted glass panel
    Rectangle {
        anchors.fill: parent
        color: "#E60E1017"
        border.color: Theme.borderGlass
        border.width: 1

        MouseArea {
            anchors.fill: parent
            // Prevent clicks from penetrating to video underneath
            hoverEnabled: true
            onWheel: (wheel) => wheel.accepted = true
        }
    }

    Column {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        // --- Header --------------------------------------------------------
        Item {
            width: parent.width
            height: 32

            Row {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Image {
                    width: 18
                    height: 18
                    source: "../icons/list.svg"
                    sourceSize: Qt.size(18, 18)
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: "لیست پخش"
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMedium
                    font.weight: Font.DemiBold
                    anchors.verticalCenter: parent.verticalCenter
                }

                Rectangle {
                    height: 20
                    width: countText.implicitWidth + 12
                    radius: 10
                    color: "#222736"
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        id: countText
                        anchors.centerIn: parent
                        text: Mpv.playlistCount + " فایل"
                        color: Theme.textSecondary
                        font.family: Theme.fontDigits
                        font.pixelSize: 11
                    }
                }
            }

            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                // Shuffle button
                Rectangle {
                    width: 28
                    height: 28
                    radius: Theme.radiusSmall
                    color: shuffleArea.containsMouse ? "#2A3042" : "transparent"

                    Image {
                        anchors.centerIn: parent
                        width: 15
                        height: 15
                        source: "../icons/rotate-ccw.svg"
                        sourceSize: Qt.size(15, 15)
                    }

                    MouseArea {
                        id: shuffleArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            Mpv.shufflePlaylist()
                            Mpv.showOsd("لیست پخش تصادفی شد (Shuffle)")
                        }
                    }
                }

                // Clear button
                Rectangle {
                    width: 28
                    height: 28
                    radius: Theme.radiusSmall
                    color: clearArea.containsMouse ? "#2A3042" : "transparent"

                    Image {
                        anchors.centerIn: parent
                        width: 15
                        height: 15
                        source: "../icons/trash-2.svg"
                        sourceSize: Qt.size(15, 15)
                    }

                    MouseArea {
                        id: clearArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            Mpv.clearPlaylist()
                            Mpv.showOsd("لیست پخش پاکسازی شد")
                        }
                    }
                }

                // Close button
                Rectangle {
                    width: 28
                    height: 28
                    radius: Theme.radiusSmall
                    color: closeArea.containsMouse ? "#2A3042" : "transparent"

                    Image {
                        anchors.centerIn: parent
                        width: 16
                        height: 16
                        source: "../icons/x.svg"
                        sourceSize: Qt.size(16, 16)
                    }

                    MouseArea {
                        id: closeArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.close()
                    }
                }
            }
        }

        // --- Search / Filter Box --------------------------------------------
        Rectangle {
            width: parent.width
            height: 34
            radius: Theme.radiusSmall
            color: "#141721"
            border.color: searchInput.activeFocus ? "#505870" : Theme.borderGlass
            border.width: 1

            Row {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                Image {
                    width: 14
                    height: 14
                    source: "../icons/search.svg"
                    sourceSize: Qt.size(14, 14)
                    anchors.verticalCenter: parent.verticalCenter
                }

                TextInput {
                    id: searchInput
                    width: parent.width - 44
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontNormal
                    selectByMouse: true
                    clip: true

                    Text {
                        text: "جستجو در لیست..."
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontNormal
                        visible: !searchInput.text && !searchInput.activeFocus
                    }

                    onTextChanged: root.searchQuery = text.trim().toLowerCase()
                }

                Image {
                    visible: searchInput.text.length > 0
                    width: 14
                    height: 14
                    source: "../icons/x.svg"
                    sourceSize: Qt.size(14, 14)
                    anchors.verticalCenter: parent.verticalCenter

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            searchInput.text = ""
                            root.searchQuery = ""
                        }
                    }
                }
            }
        }

        // --- Virtualized Track List -----------------------------------------
        ListView {
            id: playlistView
            width: parent.width
            height: parent.height - 90
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: Mpv.playlist
            spacing: 2

            ScrollBar.vertical: ScrollBar {
                active: true
                policy: ScrollBar.AsNeeded
            }

            delegate: Rectangle {
                id: itemDelegate
                width: playlistView.width
                height: isMatching ? 38 : 0
                visible: isMatching

                readonly property var entry: modelData
                readonly property int itemIdx: entry.index !== undefined ? entry.index : index
                readonly property string itemTitle: entry.title || entry.filename || ("فایل " + (itemIdx + 1))
                readonly property bool isCurrent: itemIdx === Mpv.playlistIndex
                readonly property bool isMatching: root.searchQuery.length === 0
                                                   || itemTitle.toLowerCase().indexOf(root.searchQuery) !== -1

                radius: Theme.radiusSmall
                color: isCurrent ? "#242A3D" : (itemMouseArea.containsMouse ? "#181B26" : "transparent")
                border.color: isCurrent ? "#3A4565" : "transparent"
                border.width: isCurrent ? 1 : 0

                Behavior on color { ColorAnimation { duration: 100 } }

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 8

                    // Playing indicator or index
                    Item {
                        width: 24
                        height: parent.height

                        Image {
                            anchors.centerIn: parent
                            visible: itemDelegate.isCurrent
                            width: 14
                            height: 14
                            source: Mpv.paused ? "../icons/pause.svg" : "../icons/play.svg"
                            sourceSize: Qt.size(14, 14)
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: !itemDelegate.isCurrent
                            text: String(itemDelegate.itemIdx + 1).padStart(2, "0")
                            color: Theme.textMuted
                            font.family: Theme.fontDigits
                            font.pixelSize: 11
                        }
                    }

                    // Title
                    Text {
                        width: parent.width - 64
                        anchors.verticalCenter: parent.verticalCenter
                        text: itemDelegate.itemTitle
                        color: itemDelegate.isCurrent ? Theme.textPrimary : (itemMouseArea.containsMouse ? Theme.textPrimary : Theme.textSecondary)
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontNormal
                        font.weight: itemDelegate.isCurrent ? Font.Medium : Font.Normal
                        elide: Text.ElideRight
                    }

                    // Remove item button
                    Rectangle {
                        width: 22
                        height: 22
                        anchors.verticalCenter: parent.verticalCenter
                        radius: Theme.radiusSmall
                        color: removeArea.containsMouse ? "#3A435C" : "transparent"
                        visible: itemMouseArea.containsMouse

                        Image {
                            anchors.centerIn: parent
                            width: 12
                            height: 12
                            source: "../icons/x.svg"
                            sourceSize: Qt.size(12, 12)
                        }

                        MouseArea {
                            id: removeArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Mpv.removeIndex(itemDelegate.itemIdx)
                        }
                    }
                }

                MouseArea {
                    id: itemMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    acceptedButtons: Qt.LeftButton
                    onClicked: Mpv.playIndex(itemDelegate.itemIdx)
                }
            }
        }
    }
}
