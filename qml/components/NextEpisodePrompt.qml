import QtQuick
import QtQuick.Controls
import Parvaz 1.0
import ".."

Item {
    id: root

    property bool dismissed: false
    property bool controlBarVisible: true

    width: 360
    height: 124

    anchors.right: parent.right
    anchors.rightMargin: 24
    anchors.bottom: parent.bottom
    anchors.bottomMargin: controlBarVisible ? 94 : 28

    visible: opacity > 0.0
    opacity: (Mpv.creditsZoneActive && !Mpv.isAudioOnly && !dismissed) ? 1.0 : 0.0

    Behavior on opacity {
        NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
    }

    Behavior on anchors.bottomMargin {
        NumberAnimation { duration: 200; easing.type: Easing.OutQuad }
    }

    Connections {
        target: Mpv
        function onCreditsZoneChanged() {
            if (Mpv.creditsZoneActive) {
                root.dismissed = false;
            }
        }
        function onCurrentFileChanged() {
            root.dismissed = false;
        }
    }

    // Glassmorphism Card
    Rectangle {
        id: card
        anchors.fill: parent
        radius: 12
        color: Theme.bgGlass
        border.color: Theme.borderGlass
        border.width: 1
        clip: true

        // Top Accent Progress / Highlight Line
        Rectangle {
            id: accentBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 2
            color: Theme.accentColor
        }

        // Inner Layout
        Column {
            anchors.fill: parent
            anchors.margins: 12
            anchors.topMargin: 10
            spacing: 8

            // Header Row: Icon + Title + Close Button
            Row {
                width: parent.width
                spacing: 10

                Rectangle {
                    width: 28
                    height: 28
                    radius: 7
                    color: Theme.accentSubtle
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        anchors.centerIn: parent
                        width: 16
                        height: 16
                        source: "../icons/skip-forward.svg"
                        sourceSize: Qt.size(16, 16)
                    }
                }

                Column {
                    width: parent.width - 28 - 24 - 20
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2

                    Text {
                        text: Mpv.hasNextEpisode ? "قسمت بعدی" : "تیتراژ پایانی"
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        font.bold: true
                        color: Theme.accentColor
                    }

                    Text {
                        width: parent.width
                        text: Mpv.hasNextEpisode ? (Mpv.nextEpisodeTitle || "Next Episode") : "پرش به انتهای تیتراژ"
                        font.family: Theme.fontFamily
                        font.pixelSize: 13
                        font.bold: true
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                    }
                }

                // Dismiss Button (X)
                Rectangle {
                    width: 24
                    height: 24
                    radius: 12
                    color: dismissMouse.containsMouse ? Theme.bgGlassHover : "transparent"
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        anchors.centerIn: parent
                        width: 14
                        height: 14
                        source: "../icons/x.svg"
                        sourceSize: Qt.size(14, 14)
                    }

                    MouseArea {
                        id: dismissMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.dismissed = true
                    }
                }
            }

            // Action Buttons Row
            Row {
                width: parent.width
                spacing: 8
                anchors.horizontalCenter: parent.horizontalCenter

                // Main Skip Button (S)
                Rectangle {
                    width: parent.width - 128
                    height: 36
                    radius: 8
                    color: skipMouse.containsMouse ? Qt.lighter(Theme.accentColor, 1.15) : Theme.accentColor

                    Behavior on color { ColorAnimation { duration: 120 } }

                    Row {
                        anchors.centerIn: parent
                        spacing: 6

                        Image {
                            width: 14
                            height: 14
                            anchors.verticalCenter: parent.verticalCenter
                            source: "../icons/skip-forward.svg"
                            sourceSize: Qt.size(14, 14)
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: Mpv.hasNextEpisode ? "پخش بعدی (S)" : "رد تیتراژ (S)"
                            font.family: Theme.fontFamily
                            font.pixelSize: 12
                            font.bold: true
                            color: "#FFFFFF"
                        }
                    }

                    MouseArea {
                        id: skipMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Mpv.skipCredits()
                    }
                }

                // Auto-skip toggle pill
                Rectangle {
                    width: 120
                    height: 36
                    radius: 8
                    color: Config.autoSkipCredits ? Theme.accentSubtle : Theme.bgGlassLight
                    border.color: Config.autoSkipCredits ? Theme.accentColor : Theme.borderGlass
                    border.width: 1

                    Row {
                        anchors.centerIn: parent
                        spacing: 4

                        Image {
                            width: 12
                            height: 12
                            anchors.verticalCenter: parent.verticalCenter
                            source: "../icons/check.svg"
                            sourceSize: Qt.size(12, 12)
                            visible: Config.autoSkipCredits
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "رد خودکار"
                            font.family: Theme.fontFamily
                            font.pixelSize: 11
                            color: Config.autoSkipCredits ? Theme.accentColor : Theme.textSecondary
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Config.setAutoSkipCredits(!Config.autoSkipCredits)
                    }
                }
            }
        }
    }
}
