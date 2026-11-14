import QtQuick
import QtQuick.Controls
import Parvaz 1.0
import ".."

Item {
    id: root
    width: Math.min(340, Math.round(parent.width * 0.75))

    property bool isOpen: false
    function toggle() { isOpen = !isOpen }
    function open() { isOpen = true }
    function close() { isOpen = false }

    signal addFilesRequested()

    // Smooth slide animation from right edge (anchors.right intentionally omitted so x can animate freely)
    x: isOpen ? (parent.width - width) : parent.width
    visible: isOpen || x < parent.width

    Behavior on x {
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }

    property string searchQuery: ""

    // Left shadow for crisp depth separation
    Rectangle {
        width: 14
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: panelBg.left
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 1.0; color: "#50000000" }
        }
    }

    // Solid opaque desktop surface (PotPlayer deep charcoal)
    Rectangle {
        id: panelBg
        anchors.fill: parent
        color: "#111319"
        border.color: "#222736"
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

        // --- 1. PotPlayer Desktop Header (36px) ------------------------------
        Rectangle {
            width: parent.width
            height: 36
            color: "#161822"
            border.color: "#202534"
            border.width: 1

            // Tab-like title
            Row {
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 7

                Image {
                    width: 14
                    height: 14
                    source: "../icons/list.svg"
                    sourceSize: Qt.size(14, 14)
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: "لیست پخش"
                    color: "#FFFFFF"
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontNormal
                    font.weight: Font.DemiBold
                    anchors.verticalCenter: parent.verticalCenter
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: countText.implicitWidth + 8
                    height: 18
                    radius: 3
                    color: "#222736"

                    Text {
                        id: countText
                        anchors.centerIn: parent
                        text: String(Mpv.playlistCount)
                        color: "#9EA4B8"
                        font.family: Theme.fontDigits
                        font.pixelSize: 11
                    }
                }
            }

            // Quick actions (Shuffle, Clear, Close)
            Row {
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3

                // Shuffle button
                Rectangle {
                    width: 26
                    height: 26
                    radius: 3
                    color: shuffleMouse.containsMouse ? "#293044" : "transparent"

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

                    ToolTip {
                        visible: shuffleMouse.containsMouse
                        text: "تصادفی کردن لیست"
                        delay: 400
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                    }
                }

                // Clear button
                Rectangle {
                    width: 26
                    height: 26
                    radius: 3
                    color: clearMouse.containsMouse ? "#293044" : "transparent"

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

                    ToolTip {
                        visible: clearMouse.containsMouse
                        text: "پاک کردن لیست"
                        delay: 400
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                    }
                }

                // Close button
                Rectangle {
                    width: 26
                    height: 26
                    radius: 3
                    color: closeMouse.containsMouse ? "#C53030" : "transparent"

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

                    ToolTip {
                        visible: closeMouse.containsMouse
                        text: "بستن (Esc / F6)"
                        delay: 400
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                    }
                }
            }
        }

        // --- 2. Razor-flat Desktop Search Bar (32px) -----------------------
        Rectangle {
            width: parent.width
            height: 32
            color: "#12141C"

            Rectangle {
                anchors.fill: parent
                anchors.margins: 4
                color: "#0B0C10"
                border.color: filterInput.activeFocus ? "#3B4663" : "#1E2230"
                border.width: 1
                radius: 3

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 6

                    Image {
                        width: 12
                        height: 12
                        source: "../icons/search.svg"
                        sourceSize: Qt.size(12, 12)
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    TextInput {
                        id: filterInput
                        width: parent.width - 36
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#FFFFFF"
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        selectByMouse: true
                        clip: true

                        Text {
                            text: "فیلتر کردن عناوین..."
                            color: "#5B6075"
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
            height: parent.height - 36 - 32 - 30
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
                color: isCurrent ? "#202738" : (rowMouse.containsMouse ? "#181D2A" : (itemIdx % 2 === 0 ? "#111319" : "#13161E"))

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
                        width: parent.width - 54
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

        // --- 4. Desktop Status Footer (30px) -------------------------------
        Rectangle {
            width: parent.width
            height: 30
            color: "#0E1015"
            border.color: "#1C202C"
            border.width: 1

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Text {
                    text: Mpv.playlistCount + " آیتم در صف"
                    color: "#6B7286"
                    font.family: Theme.fontDigits
                    font.pixelSize: 10
                }
            }

            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                width: addBtnRow.implicitWidth + 12
                height: 22
                radius: 3
                color: addMouse.containsMouse ? "#242A3B" : "#171B26"
                border.color: "#282F42"
                border.width: 1

                Row {
                    id: addBtnRow
                    anchors.centerIn: parent
                    spacing: 4

                    Image {
                        width: 11
                        height: 11
                        source: "../icons/list-plus.svg"
                        sourceSize: Qt.size(11, 11)
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: "افزودن فایل…"
                        color: "#E2E4EC"
                        font.family: Theme.fontFamily
                        font.pixelSize: 10
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                MouseArea {
                    id: addMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.addFilesRequested()
                }
            }
        }
    }
}
