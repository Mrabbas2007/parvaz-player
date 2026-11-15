import QtQuick
import QtQuick.Controls
import Parvaz 1.0
import ".."

Item {
    id: root
    anchors.fill: parent
    visible: isOpen

    property bool isOpen: false

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
    }

    // Semi-transparent backdrop
    Rectangle {
        anchors.fill: parent
        color: "#80000000"

        MouseArea {
            anchors.fill: parent
            onClicked: root.closeDialog()
        }
    }

    // Modal dialog box
    Rectangle {
        id: dialogBox
        width: Math.min(600, parent.width - 40)
        height: Math.min(520, parent.height - 60)
        anchors.centerIn: parent
        color: "#14161F"
        border.color: "#282D3E"
        border.width: 1
        radius: 8

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

            // --- 1. Header ---
            Row {
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
                        color: "#1E2638"
                        border.color: "#2E3A56"
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

                // Close button
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
                color: "#202534"
            }

            // --- 2. Search Input & Search Action ---
            Row {
                width: parent.width
                height: 34
                spacing: 8

                Rectangle {
                    width: parent.width - 90
                    height: parent.height
                    color: "#181B26"
                    border.color: searchField.activeFocus ? "#3B82F6" : "#282D3E"
                    border.width: 1
                    radius: 4

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
                    width: 82
                    height: parent.height
                    radius: 4
                    color: searchBtnMouse.containsMouse ? "#2563EB" : "#1D4ED8"

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
                height: 24
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
                        { label: "فارسی (FA)", code: "fa" },
                        { label: "English (EN)", code: "en" }
                    ]

                    delegate: Rectangle {
                        width: pillText.implicitWidth + 12
                        height: 22
                        radius: 11
                        anchors.verticalCenter: parent.verticalCenter
                        property bool selected: Mpv.subtitleSearch.currentLanguage === modelData.code
                        color: selected ? "#2A4365" : (pillMouse.containsMouse ? "#1E2433" : "#161922")
                        border.color: selected ? "#3182CE" : "#282D3E"
                        border.width: 1

                        Text {
                            id: pillText
                            anchors.centerIn: parent
                            text: modelData.label
                            color: parent.selected ? "#63B3ED" : Theme.textSecondary
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

                Item { width: 1; height: 1 }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: Mpv.subtitleSearch.statusMessage
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: "#202534"
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
                        active: resultsList.moving || resultsList.fllicking
                    }

                    delegate: Rectangle {
                        width: resultsList.width - 8
                        height: 52
                        radius: 5
                        color: dMouse.containsMouse ? "#1A1F2C" : "#131620"
                        border.color: dMouse.containsMouse ? "#2F384C" : "#202534"
                        border.width: 1

                        MouseArea {
                            id: dMouse
                            anchors.fill: parent
                            hoverEnabled: true
                        }

                        Row {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 10

                            // Language badge
                            Rectangle {
                                width: 34
                                height: 24
                                radius: 3
                                anchors.verticalCenter: parent.verticalCenter
                                color: modelData.langCode === "fa" ? "#422800" : "#0D2538"
                                border.color: modelData.langCode === "fa" ? "#B7791F" : "#2B6CB0"
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.langCode ? modelData.langCode.toUpperCase() : "SUB"
                                    color: modelData.langCode === "fa" ? "#ECC94B" : "#90CDF4"
                                    font.family: Theme.fontDigits
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }

                            // Subtitle Details
                            Column {
                                width: parent.width - 190
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 3

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
                                        text: "✓ Exact Hash Match"
                                        color: "#38A169"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: 10
                                        font.bold: true
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

                                    Text {
                                        visible: modelData.rating > 0
                                        text: "★ " + modelData.rating.toFixed(1)
                                        color: "#D69E2E"
                                        font.family: Theme.fontDigits
                                        font.pixelSize: 11
                                    }
                                }
                            }

                            // Download & Apply Button
                            Rectangle {
                                width: 120
                                height: 28
                                anchors.verticalCenter: parent.verticalCenter
                                radius: 4
                                color: dlMouse.containsMouse ? "#2B6CB0" : "#1E3A8A"

                                Text {
                                    anchors.centerIn: parent
                                    text: (Mpv.subtitleSearch.downloading) ? "Downloading…" : "Download & Apply"
                                    color: "#FFFFFF"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSmall
                                    font.bold: true
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
