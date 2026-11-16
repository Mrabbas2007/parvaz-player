import QtQuick
import QtQuick.Controls
import Parvaz 1.0
import ".."

Item {
    id: root
    anchors.fill: parent
    visible: isOpen

    property bool isOpen: false
    signal closed()

    function openDialog() {
        isOpen = true;
        if (Mpv.currentFile.length > 0) {
            Mpv.subtitleSearch.searchForCurrentFile(Mpv.currentFile);
            searchField.text = Mpv.subtitleSearch.currentQuery;
        }
        searchField.forceActiveFocus();
    }

    function closeDialog() {
        isOpen = false;
        Mpv.subtitleSearch.cancelSearch();
        root.closed();
    }

    // Modal Dim Backdrop
    Rectangle {
        anchors.fill: parent
        color: "#B3000000"

        MouseArea {
            anchors.fill: parent
            onClicked: root.closeDialog()
        }
    }

    // Modal Dialog Box (100% Solid Studio Opaque Surface)
    Rectangle {
        id: dialogBox
        width: Math.min(620, parent.width - 40)
        height: Math.min(540, parent.height - 60)
        anchors.centerIn: parent
        color: "#12141C"
        border.color: "#242938"
        border.width: 1
        radius: 10
        clip: true

        // Prevent click-through
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onWheel: (wheel) => wheel.accepted = true
        }

        Column {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            // --- 1. Header (Clean & Solid) ---
            Item {
                width: parent.width
                height: 28

                Row {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    Image {
                        width: 16
                        height: 16
                        source: "../icons/subtitles.svg"
                        sourceSize: Qt.size(16, 16)
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: "Search Online Subtitles"
                        color: "#FFFFFF"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMedium
                        font.bold: true
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Rectangle {
                        visible: Mpv.subtitleSearch.fileHash.length > 0
                        width: hashText.implicitWidth + 8
                        height: 18
                        radius: 3
                        color: "#181D2A"
                        border.color: "#28334A"
                        border.width: 1
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            id: hashText
                            anchors.centerIn: parent
                            text: "Hash: " + Mpv.subtitleSearch.fileHash.substring(0, 8) + "…"
                            color: "#8E99B3"
                            font.family: Theme.fontDigits
                            font.pixelSize: 10
                        }
                    }
                }

                // Close button (X)
                IconButton {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "x"
                    buttonSize: 26
                    iconSize: 14
                    tooltipText: "Close (Escape)"
                    onClicked: root.closeDialog()
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: "#1E2332"
            }

            // --- 2. Search Input & Search Action ---
            Row {
                width: parent.width
                height: 36
                spacing: 8

                Rectangle {
                    width: parent.width - 92
                    height: parent.height
                    color: "#171924"
                    border.color: searchField.activeFocus ? "#3B82F6" : "#282E40"
                    border.width: 1
                    radius: 6

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 8
                        spacing: 8

                        Image {
                            width: 14
                            height: 14
                            source: "../icons/search.svg"
                            sourceSize: Qt.size(14, 14)
                            anchors.verticalCenter: parent.verticalCenter
                            opacity: 0.6
                        }

                        TextInput {
                            id: searchField
                            width: parent.width - 50
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#FFFFFF"
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontNormal
                            selectByMouse: true
                            clip: true
                            onAccepted: doSearch()
                        }

                        IconButton {
                            visible: searchField.text.length > 0
                            anchors.verticalCenter: parent.verticalCenter
                            iconName: "x"
                            buttonSize: 20
                            iconSize: 12
                            tooltipText: "Clear"
                            onClicked: { searchField.text = ""; searchField.forceActiveFocus(); }
                        }
                    }
                }

                // Search Button
                Rectangle {
                    width: 84
                    height: parent.height
                    radius: 6
                    color: searchBtnMouse.containsMouse ? "#2563EB" : "#1D4ED8"

                    Behavior on color { ColorAnimation { duration: 120 } }

                    Text {
                        anchors.centerIn: parent
                        text: "Search"
                        color: "#FFFFFF"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontNormal
                        font.bold: true
                    }

                    MouseArea {
                        id: searchBtnMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: doSearch()
                    }
                }
            }

            // --- 3. Language Filter Pills & Status ---
            Row {
                width: parent.width
                height: 26
                spacing: 8

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Language:"
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                }

                Repeater {
                    model: [
                        { label: "All", code: "all" },
                        { label: "Persian (FA)", code: "fa" },
                        { label: "English (EN)", code: "en" }
                    ]

                    delegate: Rectangle {
                        width: pillText.implicitWidth + 14
                        height: 24
                        radius: 12
                        anchors.verticalCenter: parent.verticalCenter
                        property bool selected: Mpv.subtitleSearch.currentLanguage === modelData.code
                        color: selected ? "#1E2B45" : (pillMouse.containsMouse ? "#1C202E" : "#141722")
                        border.color: selected ? "#3B82F6" : "#242938"
                        border.width: 1

                        Text {
                            id: pillText
                            anchors.centerIn: parent
                            text: modelData.label
                            color: parent.selected ? "#60A5FA" : Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall
                            font.bold: parent.selected
                        }

                        MouseArea {
                            id: pillMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                Mpv.subtitleSearch.searchByQuery(searchField.text, modelData.code);
                            }
                        }
                    }
                }

                Item { width: 4; height: 1 }

                // Status Text Container (Bounded, Never Overflows!)
                Item {
                    width: Math.max(100, parent.width - 310)
                    height: parent.height
                    clip: true

                    Text {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: Mpv.subtitleSearch.statusMessage
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                        elide: Text.ElideRight
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: "#1E2332"
            }

            // --- 4. Results List View ---
            Item {
                width: parent.width
                height: parent.height - y - 8

                // Empty / Loading state
                Column {
                    anchors.centerIn: parent
                    spacing: 10
                    visible: Mpv.subtitleSearch.results.length === 0

                    Image {
                        width: 32
                        height: 32
                        source: "../icons/subtitles.svg"
                        sourceSize: Qt.size(32, 32)
                        anchors.horizontalCenter: parent.horizontalCenter
                        opacity: 0.3
                    }

                    Text {
                        text: Mpv.subtitleSearch.searching
                                ? "Searching online databases for matching subtitles…"
                                : "No subtitles loaded. Enter a title and press Search."
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontNormal
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                ListView {
                    id: resultsList
                    anchors.fill: parent
                    clip: true
                    spacing: 6
                    model: Mpv.subtitleSearch.results
                    visible: Mpv.subtitleSearch.results.length > 0

                    ScrollBar.vertical: ScrollBar {
                        active: resultsList.moving || resultsList.flicking
                    }

                    delegate: Rectangle {
                        id: resultDelegate
                        width: resultsList.width - 8
                        height: 56
                        radius: 6
                        color: dMouse.containsMouse ? "#1B202D" : "#151823"
                        border.color: dMouse.containsMouse ? "#2E384D" : "#1E2332"
                        border.width: 1

                        readonly property bool isAppliedThis: Mpv.subtitleSearch.appliedIndex === index
                        readonly property bool isDownloadingThis: Mpv.subtitleSearch.downloading && Mpv.subtitleSearch.downloadingIndex === index

                        MouseArea {
                            id: dMouse
                            anchors.fill: parent
                            hoverEnabled: true
                        }

                        // Language Badge (Left edge)
                        Rectangle {
                            id: langBadge
                            width: 38
                            height: 26
                            radius: 4
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            color: modelData.langCode === "fa" ? "#3D2708" : (modelData.langCode === "en" ? "#0C243B" : "#1A1F2C")
                            border.color: modelData.langCode === "fa" ? "#B7791F" : (modelData.langCode === "en" ? "#2B6CB0" : "#2E374D")
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: modelData.langCode ? modelData.langCode.toUpperCase() : "SUB"
                                color: modelData.langCode === "fa" ? "#F6AD55" : (modelData.langCode === "en" ? "#90CDF4" : "#A0AEC0")
                                font.family: Theme.fontDigits
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }

                        // Download & Apply Action Button (Right edge - 100% visible and anchored)
                        Rectangle {
                            id: dlActionBtn
                            width: resultDelegate.isAppliedThis ? 116 : 124
                            height: 30
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            radius: 5
                            color: resultDelegate.isAppliedThis
                                   ? (dlMouse.containsMouse ? "#059669" : "#10B981")
                                   : (resultDelegate.isDownloadingThis
                                      ? "#374151"
                                      : (dlMouse.containsMouse ? "#2563EB" : "#1D4ED8"))
                            border.color: resultDelegate.isAppliedThis ? "#34D399" : "transparent"
                            border.width: resultDelegate.isAppliedThis ? 1 : 0

                            Behavior on color { ColorAnimation { duration: 120 } }

                            Row {
                                anchors.centerIn: parent
                                spacing: 6

                                Image {
                                    width: 12
                                    height: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    source: resultDelegate.isAppliedThis ? "../icons/check.svg" : "../icons/subtitles.svg"
                                    sourceSize: Qt.size(12, 12)
                                    visible: !resultDelegate.isDownloadingThis
                                }

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: resultDelegate.isAppliedThis
                                          ? "✓ Active"
                                          : (resultDelegate.isDownloadingThis ? "Downloading…" : "Download & Apply")
                                    color: "#FFFFFF"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSmall
                                    font.bold: true
                                }
                            }

                            MouseArea {
                                id: dlMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    Mpv.subtitleSearch.downloadAndApply(index);
                                }
                            }
                        }

                        // Subtitle Title & Metadata (Middle - spans cleanly between Badge and Button)
                        Column {
                            anchors.left: langBadge.right
                            anchors.leftMargin: 10
                            anchors.right: dlActionBtn.left
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 3
                            clip: true

                            Text {
                                width: parent.width
                                text: modelData.release || modelData.fileName || "Subtitle Track"
                                color: "#FFFFFF"
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontNormal
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            Row {
                                spacing: 8

                                Text {
                                    visible: modelData.isExactMatch
                                    text: "✓ Exact Match"
                                    color: "#38A169"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 10
                                    font.bold: true
                                }

                                Text {
                                    text: modelData.source ? ("• " + modelData.source) : ""
                                    color: "#4A5568"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 10
                                }

                                Text {
                                    text: modelData.language || ""
                                    color: Theme.textMuted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 11
                                }

                                Text {
                                    visible: modelData.downloads > 0
                                    text: "↓ " + modelData.downloads
                                    color: Theme.textMuted
                                    font.family: Theme.fontDigits
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    function doSearch() {
        if (searchField.text.trim().length > 0) {
            Mpv.subtitleSearch.searchByQuery(searchField.text.trim(), Mpv.subtitleSearch.currentLanguage);
        } else if (Mpv.currentFile.length > 0) {
            Mpv.subtitleSearch.searchForCurrentFile(Mpv.currentFile);
        }
    }

    Keys.onEscapePressed: {
        root.closeDialog();
    }
}
