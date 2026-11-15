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
        var clip = Mpv.clipboardText();
        var norm = Mpv.normalizeStreamUrl(clip);
        if (Mpv.isValidStreamUrl(norm)) {
            urlField.text = norm;
        } else {
            urlField.text = "";
        }
        urlField.forceActiveFocus();
    }

    function closeDialog() {
        isOpen = false;
    }

    function playCurrent() {
        var u = Mpv.normalizeStreamUrl(urlField.text);
        if (u.length > 0) {
            Mpv.openUrl(u);
            closeDialog();
        }
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
        width: Math.min(560, parent.width - 40)
        height: Math.min((Mpv.recentUrls.length > 0 ? 380 : 250) + (!Mpv.hasYtdl ? 40 : 0), parent.height - 40)
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
            anchors.margins: 18
            spacing: 14

            // --- 1. Header ---
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
                        source: "../icons/folder-open.svg"
                        sourceSize: Qt.size(16, 16)
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: "Open Stream / URL (yt-dlp)"
                        color: "#FFFFFF"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMedium
                        font.bold: true
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

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

            // Warning notice when yt-dlp is not installed
            Rectangle {
                width: parent.width
                height: 32
                radius: 4
                color: "#2D1D10"
                border.color: "#8C5015"
                border.width: 1
                visible: !Mpv.hasYtdl

                Row {
                    anchors.centerIn: parent
                    spacing: 6

                    Text {
                        text: "⚠️ yt-dlp not found: Install via 'sudo pacman -S yt-dlp' to play YouTube"
                        color: "#F6AD55"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                    }
                }
            }

            // --- 2. URL Input Field ---
            Column {
                width: parent.width
                spacing: 6

                Text {
                    text: "Enter video, audio or streaming URL (YouTube, Twitch, HLS, DASH, direct link):"
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                }

                Rectangle {
                    width: parent.width
                    height: 38
                    color: "#181B26"
                    border.color: urlField.activeFocus ? "#3B82F6" : "#282D3E"
                    border.width: 1
                    radius: 4

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 8
                        spacing: 8

                        TextInput {
                            id: urlField
                            width: parent.width - (pasteBtn.visible ? 90 : 30)
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#FFFFFF"
                            font.family: Theme.fontDigits
                            font.pixelSize: Theme.fontNormal
                            selectByMouse: true
                            clip: true
                            onAccepted: root.playCurrent()
                        }

                        // Quick Paste Button
                        Rectangle {
                            id: pasteBtn
                            width: 76
                            height: 26
                            anchors.verticalCenter: parent.verticalCenter
                            radius: 3
                            color: pasteMouse.containsMouse ? "#2A3348" : "#1F2533"
                            border.color: "#323B50"
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "Paste (📋)"
                                color: pasteMouse.containsMouse ? "#FFFFFF" : Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSmall
                            }

                            MouseArea {
                                id: pasteMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    var clip = Mpv.clipboardText();
                                    var norm = Mpv.normalizeStreamUrl(clip);
                                    if (norm.length > 0) {
                                        urlField.text = norm;
                                        urlField.forceActiveFocus();
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // --- 3. Recent Streams (if available) ---
            Column {
                width: parent.width
                spacing: 6
                visible: Mpv.recentUrls.length > 0

                Item {
                    width: parent.width
                    height: 18

                    Text {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Recent URLs:"
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Clear history"
                        color: clearHistMouse.containsMouse ? "#EF4444" : Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: 11

                        MouseArea {
                            id: clearHistMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Mpv.clearRecentUrls()
                        }
                    }
                }

                ListView {
                    width: parent.width
                    height: Math.min(100, Mpv.recentUrls.length * 26)
                    clip: true
                    model: Mpv.recentUrls
                    spacing: 2

                    delegate: Rectangle {
                        width: parent.width
                        height: 24
                        radius: 3
                        color: rUrlMouse.containsMouse ? "#1E2433" : "transparent"

                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData
                            color: rUrlMouse.containsMouse ? "#FFFFFF" : Theme.textSecondary
                            font.family: Theme.fontDigits
                            font.pixelSize: Theme.fontSmall
                            elide: Text.ElideMiddle
                        }

                        MouseArea {
                            id: rUrlMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                urlField.text = modelData;
                                root.playCurrent();
                            }
                        }
                    }
                }
            }

            Item { width: 1; height: 1 }

            // --- 4. Action Buttons ---
            Row {
                anchors.right: parent.right
                spacing: 10

                // Cancel Button
                Rectangle {
                    width: 76
                    height: 32
                    radius: 4
                    color: cancelMouse.containsMouse ? "#2A3348" : "transparent"
                    border.color: "#282E3E"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "Cancel"
                        color: cancelMouse.containsMouse ? "#FFFFFF" : Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontNormal
                    }

                    MouseArea {
                        id: cancelMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.closeDialog()
                    }
                }

                // Play Button
                Rectangle {
                    width: 110
                    height: 32
                    radius: 4
                    color: playBtnMouse.containsMouse ? "#2563EB" : "#1D4ED8"

                    Text {
                        anchors.centerIn: parent
                        text: "Play Stream"
                        color: "#FFFFFF"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontNormal
                        font.bold: true
                    }

                    MouseArea {
                        id: playBtnMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.playCurrent()
                    }
                }
            }
        }
    }

    Keys.onEscapePressed: {
        root.closeDialog();
    }
}
