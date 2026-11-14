import QtQuick
import QtQuick.Controls
import Parvaz 1.0
import ".."

Item {
    id: root
    width: Math.min(320, Math.round(parent.width * 0.75))
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
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }

    property string searchQuery: ""

    // Left shadow for depth separation
    Rectangle {
        width: 12
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: panelBg.left
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 1.0; color: "#40000000" }
        }
    }

    // Solid opaque desktop surface (prevents video stripes bleed-through)
    Rectangle {
        id: panelBg
        anchors.fill: parent
        color: "#0F1014"
        border.color: "#222530"
        border.width: 1

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onWheel: (wheel) => wheel.accepted = true
        }
    }

    Column {
        anchors.fill: parent
        spacing: 0

        // --- 1. Compact Desktop Header (34px) ------------------------------
        Rectangle {
            width: parent.width
            height: 34
            color: "#13141B"
            border.color: "#1C1F28"
            border.width: 1

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                Image {
                    width: 14
                    height: 14
                    source: "../icons/list.svg"
                    sourceSize: Qt.size(14, 14)
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: "لیست پخش"
                    color: "#E2E4EC"
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontNormal
                    font.weight: Font.DemiBold
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: "(" + Mpv.playlistCount + ")"
                    color: "#6E7385"
                    font.family: Theme.fontDigits
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                // Shuffle button
                Rectangle {
                    width: 24
                    height: 24
                    radius: 3
                    color: shuffleMouse.containsMouse ? "#222634" : "transparent"

                    Image {
                        anchors.centerIn: parent
                        width: 13
                        height: 13
                        source: "../icons/rotate-ccw.svg"
                        sourceSize: Qt.size(13, 13)
                    }

                    MouseArea {
                        id: shuffleMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            Mpv.shufflePlaylist()
                            Mpv.showOsd("لیست پخش تصادفی شد")
                        }
                    }
                }

                // Clear button
                Rectangle {
                    width: 24
                    height: 24
                    radius: 3
                    color: clearMouse.containsMouse ? "#222634" : "transparent"

                    Image {
                        anchors.centerIn: parent
                        width: 13
                        height: 13
                        source: "../icons/trash-2.svg"
                        sourceSize: Qt.size(13, 13)
                    }

                    MouseArea {
                        id: clearMouse
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
                    width: 24
                    height: 24
                    radius: 3
                    color: closeMouse.containsMouse ? "#222634" : "transparent"

                    Image {
                        anchors.centerIn: parent
                        width: 13
                        height: 13
                        source: "../icons/x.svg"
                        sourceSize: Qt.size(13, 13)
                    }

                    MouseArea {
                        id: closeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.close()
                    }
                }
            }
        }

        // --- 2. Razor-flat Desktop Search Bar (30px) -----------------------
        Rectangle {
            width: parent.width
            height: 30
            color: "#101115"

            Rectangle {
                anchors.fill: parent
                anchors.margins: 4
                color: "#08090C"
                border.color: filterInput.activeFocus ? "#3A4259" : "#1C1F28"
                border.width: 1
                radius: 2

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    spacing: 6

                    Image {
                        width: 11
                        height: 11
                        source: "../icons/search.svg"
                        sourceSize: Qt.size(11, 11)
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    TextInput {
                        id: filterInput
                        width: parent.width - 32
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#FFFFFF"
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        selectByMouse: true
                        clip: true

                        Text {
                            text: "فیلتر کردن عناوین..."
                            color: "#505465"
                            font.family: Theme.fontFamily
                            font.pixelSize: 11
                            visible: !filterInput.text && !filterInput.activeFocus
                        }

                        onTextChanged: root.searchQuery = text.trim().toLowerCase()
                    }

                    Image {
                        visible: filterInput.text.length > 0
                        width: 11
                        height: 11
                        source: "../icons/x.svg"
                        sourceSize: Qt.size(11, 11)
                        anchors.verticalCenter: parent.verticalCenter

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                filterInput.text = ""
                                root.searchQuery = ""
                            }
                        }
                    }
                }
            }
        }

        // --- 3. High-Density Tabular Track List -----------------------------
        ListView {
            id: playlistView
            width: parent.width
            height: parent.height - 34 - 30 - 22
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: Mpv.playlist
            spacing: 0

            ScrollBar.vertical: ScrollBar {
                active: true
                policy: ScrollBar.AsNeeded
                width: 6
            }

            delegate: Rectangle {
                id: rowDelegate
                width: playlistView.width
                height: isMatching ? 28 : 0
                visible: isMatching

                readonly property var entry: modelData
                readonly property int itemIdx: entry.index !== undefined ? entry.index : index
                readonly property string itemTitle: entry.title || entry.filename || ("فایل " + (itemIdx + 1))
                readonly property bool isCurrent: itemIdx === Mpv.playlistIndex
                readonly property bool isMatching: root.searchQuery.length === 0
                                                   || itemTitle.toLowerCase().indexOf(root.searchQuery) !== -1

                // Desktop row styling: subtle zebra stripes + clean hover/current highlight
                color: isCurrent ? "#1E2333" : (rowMouse.containsMouse ? "#181B26" : (itemIdx % 2 === 0 ? "#0F1014" : "#121319"))

                // Active track 2px vertical accent bar on left edge
                Rectangle {
                    width: 2
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    color: "#FFFFFF"
                    visible: rowDelegate.isCurrent
                }

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    spacing: 6

                    // Track Index or Playing status
                    Item {
                        width: 24
                        height: parent.height

                        Image {
                            anchors.centerIn: parent
                            visible: rowDelegate.isCurrent
                            width: 11
                            height: 11
                            source: Mpv.paused ? "../icons/pause.svg" : "../icons/play.svg"
                            sourceSize: Qt.size(11, 11)
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: !rowDelegate.isCurrent
                            text: String(rowDelegate.itemIdx + 1).padStart(2, "0") + "."
                            color: "#5C6073"
                            font.family: Theme.fontDigits
                            font.pixelSize: 10
                        }
                    }

                    // Track Title
                    Text {
                        width: parent.width - 52
                        anchors.verticalCenter: parent.verticalCenter
                        text: rowDelegate.itemTitle
                        color: rowDelegate.isCurrent ? "#FFFFFF" : (rowMouse.containsMouse ? "#EEEEEE" : "#AFB2C0")
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        font.weight: rowDelegate.isCurrent ? Font.Medium : Font.Normal
                        elide: Text.ElideRight
                    }

                    // Remove icon (visible on hover)
                    Rectangle {
                        width: 18
                        height: 18
                        anchors.verticalCenter: parent.verticalCenter
                        radius: 2
                        color: removeMouse.containsMouse ? "#303548" : "transparent"
                        visible: rowMouse.containsMouse

                        Image {
                            anchors.centerIn: parent
                            width: 10
                            height: 10
                            source: "../icons/x.svg"
                            sourceSize: Qt.size(10, 10)
                        }

                        MouseArea {
                            id: removeMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Mpv.removeIndex(rowDelegate.itemIdx)
                        }
                    }
                }

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    acceptedButtons: Qt.LeftButton
                    onClicked: Mpv.playIndex(rowDelegate.itemIdx)
                }
            }
        }

        // --- 4. Desktop Status Footer (22px) -------------------------------
        Rectangle {
            width: parent.width
            height: 22
            color: "#0C0D11"
            border.color: "#181A22"
            border.width: 1

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Text {
                    text: Mpv.playlistCount + " آیتم در صف"
                    color: "#555A6B"
                    font.family: Theme.fontDigits
                    font.pixelSize: 10
                }
            }
        }
    }
}
