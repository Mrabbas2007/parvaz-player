import QtQuick
import QtQuick.Controls
import Parvaz 1.0
import ".."

Item {
    id: root

    property bool isOpen: false

    function toggle() {
        isOpen = !isOpen;
        if (isOpen)
            root.forceActiveFocus();
    }
    function open() { isOpen = true; root.forceActiveFocus(); }
    function close() { isOpen = false; }

    visible: opacity > 0.0
    opacity: isOpen ? 1.0 : 0.0
    Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }

    anchors.fill: parent
    z: 50

    // Backdrop click-eater outside panel (clicking outside closes the center)
    MouseArea {
        anchors.fill: parent
        enabled: root.isOpen
        onClicked: root.close()
    }

    // Draggable Floating Panel
    Rectangle {
        id: panel
        width: 480
        height: 440
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)

        color: "#F20E1017"
        border.color: "#282D3E"
        border.width: 1
        radius: Theme.radiusLarge
        clip: true

        // Stop clicks from bubbling through to the backdrop
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onClicked: {}
        }

        // ---- 1. Panel Header (Draggable) ----
        Rectangle {
            id: header
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 40
            color: "#161922"
            border.color: "#202533"
            border.width: 1

            MouseArea {
                id: dragArea
                anchors.fill: parent
                anchors.rightMargin: 40
                property point clickPos: "0,0"
                cursorShape: Qt.SizeAllCursor

                onPressed: (mouse) => {
                    clickPos = Qt.point(mouse.x, mouse.y);
                }
                onPositionChanged: (mouse) => {
                    var delta = Qt.point(mouse.x - clickPos.x, mouse.y - clickPos.y);
                    panel.x = Math.max(8, Math.min(root.width - panel.width - 8, panel.x + delta.x));
                    panel.y = Math.max(8, Math.min(root.height - panel.height - 8, panel.y + delta.y));
                }
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Image {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 16
                    height: 16
                    source: "../icons/sliders.svg"
                    sourceSize: Qt.size(16, 16)
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Control Center (F7)"
                    color: "#FFFFFF"
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                }
            }

            // Close button (X)
            IconButton {
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                iconName: "x"
                tooltipText: "Close (F7 / Esc)"
                buttonSize: 28
                iconSize: 15
                onClicked: root.close()
            }
        }

        // ---- 2. Navigation Tabs ----
        property int currentTab: 0

        Row {
            id: tabRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: header.bottom
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.topMargin: 10
            height: 32
            spacing: 6

            Repeater {
                model: [
                    { name: "Video", icon: "film" },
                    { name: "Audio & EQ", icon: "music" },
                    { name: "Subtitles", icon: "subtitles" },
                    { name: "Playback", icon: "play" }
                ]

                delegate: Rectangle {
                    width: (tabRow.width - (3 * tabRow.spacing)) / 4
                    height: 32
                    radius: Theme.radiusSmall
                    color: panel.currentTab === index ? "#2A3348" : (tabMouse.containsMouse ? Theme.bgGlassHover : "#181B26")
                    border.color: panel.currentTab === index ? "#405070" : "#242938"
                    border.width: 1

                    Row {
                        anchors.centerIn: parent
                        spacing: 6

                        Image {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 14
                            height: 14
                            source: "../icons/" + modelData.icon + ".svg"
                            sourceSize: Qt.size(14, 14)
                            opacity: panel.currentTab === index ? 1.0 : 0.7
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.name
                            color: panel.currentTab === index ? "#FFFFFF" : Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall
                            font.bold: panel.currentTab === index
                        }
                    }

                    MouseArea {
                        id: tabMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: panel.currentTab = index
                    }
                }
            }
        }

        // ---- 3. Tab Contents Area ----
        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: tabRow.bottom
            anchors.bottom: parent.bottom
            anchors.margins: 12

            // ==========================================
            // TAB 0: VIDEO ADJUSTMENTS
            // ==========================================
            Item {
                id: videoTab
                anchors.fill: parent
                visible: panel.currentTab === 0

                component VideoSliderRow: Row {
                    id: vRow
                    property string label: ""
                    property real val: 0.0
                    signal valueChanged(real newVal)
                    signal resetRequested()

                    width: parent.width
                    height: 36
                    spacing: 8

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 78
                        text: vRow.label
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontNormal
                    }

                    // Slider track
                    Item {
                        anchors.verticalCenter: parent.verticalCenter
                        width: vRow.width - 78 - 50 - 32 - 24
                        height: 24

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            height: 4
                            radius: 2
                            color: "#222736"

                            // Center tick (0 mark)
                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.verticalCenter: parent.verticalCenter
                                width: 2
                                height: 8
                                color: "#444B60"
                            }

                            // Active Fill from center
                            Rectangle {
                                property real centerPos: parent.width / 2
                                property real targetPos: centerPos + (vRow.val / 100.0) * centerPos
                                x: Math.min(centerPos, targetPos)
                                width: Math.abs(targetPos - centerPos)
                                anchors.verticalCenter: parent.verticalCenter
                                height: 4
                                radius: 2
                                color: "#4A90E2"
                            }
                        }

                        // Slider Handle
                        Rectangle {
                            x: (parent.width / 2) + (vRow.val / 100.0) * (parent.width / 2) - width / 2
                            anchors.verticalCenter: parent.verticalCenter
                            width: 14
                            height: 14
                            radius: 7
                            color: sMouse.containsMouse ? "#FFFFFF" : "#E0E3EC"
                            border.color: "#3A7BD5"
                            border.width: 1
                        }

                        MouseArea {
                            id: sMouse
                            anchors.fill: parent
                            hoverEnabled: true

                            function updateFromMouse(mouseX) {
                                var ratio = (mouseX - width / 2) / (width / 2);
                                ratio = Math.max(-1.0, Math.min(1.0, ratio));
                                var rounded = Math.round(ratio * 100);
                                vRow.valueChanged(rounded);
                            }

                            onPressed: (mouse) => updateFromMouse(mouse.x)
                            onPositionChanged: (mouse) => {
                                if (pressed) updateFromMouse(mouse.x);
                            }
                        }
                    }

                    // Value Readout
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 44
                        horizontalAlignment: Text.AlignRight
                        text: (vRow.val > 0 ? "+" : "") + vRow.val.toFixed(0)
                        color: vRow.val !== 0 ? "#FFFFFF" : Theme.textMuted
                        font.family: Theme.fontDigits
                        font.pixelSize: Theme.fontNormal
                        font.bold: vRow.val !== 0
                    }

                    // Reset Button
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 24
                        height: 24
                        radius: Theme.radiusSmall
                        color: rMouse.containsMouse ? "#2A3348" : "transparent"

                        Text {
                            anchors.centerIn: parent
                            text: "↺"
                            color: vRow.val !== 0 ? "#FFFFFF" : "#555A68"
                            font.pixelSize: 13
                        }

                        MouseArea {
                            id: rMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: vRow.resetRequested()
                        }
                    }
                }

                Column {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    spacing: 4

                    VideoSliderRow {
                        label: "Brightness"
                        val: Mpv.brightness
                        onValueChanged: (v) => { Mpv.setBrightness(v); Config.setBrightness(v); }
                        onResetRequested: { Mpv.setBrightness(0); Config.setBrightness(0); }
                    }

                    VideoSliderRow {
                        label: "Contrast"
                        val: Mpv.contrast
                        onValueChanged: (v) => { Mpv.setContrast(v); Config.setContrast(v); }
                        onResetRequested: { Mpv.setContrast(0); Config.setContrast(0); }
                    }

                    VideoSliderRow {
                        label: "Saturation"
                        val: Mpv.saturation
                        onValueChanged: (v) => { Mpv.setSaturation(v); Config.setSaturation(v); }
                        onResetRequested: { Mpv.setSaturation(0); Config.setSaturation(0); }
                    }

                    VideoSliderRow {
                        label: "Gamma"
                        val: Mpv.gamma
                        onValueChanged: (v) => { Mpv.setGamma(v); Config.setGamma(v); }
                        onResetRequested: { Mpv.setGamma(0); Config.setGamma(0); }
                    }

                    VideoSliderRow {
                        label: "Hue"
                        val: Mpv.hue
                        onValueChanged: (v) => { Mpv.setHue(v); Config.setHue(v); }
                        onResetRequested: { Mpv.setHue(0); Config.setHue(0); }
                    }
                }

                // Reset All Video Button
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 220
                    height: 30
                    radius: Theme.radiusSmall
                    color: resetAllMouse.containsMouse ? "#2A3348" : "#1C202C"
                    border.color: "#2C3244"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "Reset All Video Adjustments"
                        color: resetAllMouse.containsMouse ? "#FFFFFF" : Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                    }

                    MouseArea {
                        id: resetAllMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            Mpv.resetVideoAdjustments();
                            Config.setBrightness(0);
                            Config.setContrast(0);
                            Config.setSaturation(0);
                            Config.setGamma(0);
                            Config.setHue(0);
                        }
                    }
                }
            }

            // ==========================================
            // TAB 1: AUDIO & 10-BAND EQUALIZER
            // ==========================================
            Item {
                id: audioTab
                anchors.fill: parent
                visible: panel.currentTab === 1

                // Top Toggles: Equalizer Switch & Normalizer
                Row {
                    id: eqToggles
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 28
                    spacing: 20

                    // Equalizer On/Off
                    Row {
                        spacing: 8
                        anchors.verticalCenter: parent.verticalCenter

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 36
                            height: 18
                            radius: 9
                            color: Mpv.equalizerEnabled ? "#3A7BD5" : "#222736"

                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                x: Mpv.equalizerEnabled ? parent.width - width - 2 : 2
                                width: 14
                                height: 14
                                radius: 7
                                color: "#FFFFFF"
                                Behavior on x { NumberAnimation { duration: Theme.animFast } }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    var next = !Mpv.equalizerEnabled;
                                    Mpv.setEqualizerEnabled(next);
                                    Config.setEqualizerEnabled(next);
                                }
                            }
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Equalizer"
                            color: Mpv.equalizerEnabled ? "#FFFFFF" : Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontNormal
                            font.bold: true
                        }
                    }

                    // Night Mode Normalizer
                    Row {
                        spacing: 8
                        anchors.verticalCenter: parent.verticalCenter

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 36
                            height: 18
                            radius: 9
                            color: Mpv.audioNormalizer ? "#10B981" : "#222736"

                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                x: Mpv.audioNormalizer ? parent.width - width - 2 : 2
                                width: 14
                                height: 14
                                radius: 7
                                color: "#FFFFFF"
                                Behavior on x { NumberAnimation { duration: Theme.animFast } }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    var next = !Mpv.audioNormalizer;
                                    Mpv.setAudioNormalizer(next);
                                    Config.setAudioNormalizer(next);
                                }
                            }
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Night Mode (Normalizer)"
                            color: Mpv.audioNormalizer ? "#FFFFFF" : Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontNormal
                        }
                    }
                }

                // Presets Bar
                Flickable {
                    id: presetsFlick
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: eqToggles.bottom
                    anchors.topMargin: 8
                    height: 26
                    contentWidth: presetRow.width
                    clip: true

                    Row {
                        id: presetRow
                        spacing: 4

                        Repeater {
                            model: ["Flat", "Rock", "Pop", "Jazz", "Classical", "Bass Boost", "Treble Boost", "Vocal"]

                            delegate: Rectangle {
                                width: pText.implicitWidth + 14
                                height: 24
                                radius: Theme.radiusSmall
                                color: Mpv.equalizerPreset === modelData ? "#2A3348" : (pMouse.containsMouse ? Theme.bgGlassHover : "#181A24")
                                border.color: Mpv.equalizerPreset === modelData ? "#4A90E2" : "#262B3A"
                                border.width: 1

                                Text {
                                    id: pText
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: Mpv.equalizerPreset === modelData ? "#FFFFFF" : Theme.textSecondary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSmall
                                    font.bold: Mpv.equalizerPreset === modelData
                                }

                                MouseArea {
                                    id: pMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: {
                                        Mpv.applyEqualizerPreset(modelData);
                                        Config.setEqualizerPreset(modelData);
                                        Config.setEqualizerGains(Mpv.equalizerGains);
                                    }
                                }
                            }
                        }
                    }
                }

                // 10-Band Vertical Sliders Area
                Item {
                    id: bandsContainer
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: presetsFlick.bottom
                    anchors.topMargin: 12
                    height: 175
                    opacity: Mpv.equalizerEnabled ? 1.0 : 0.4
                    Behavior on opacity { NumberAnimation { duration: Theme.animFast } }

                    property var freqLabels: ["31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"]

                    Row {
                        anchors.fill: parent

                        Repeater {
                            model: 10

                            delegate: Item {
                                width: bandsContainer.width / 10
                                height: bandsContainer.height

                                property real currentGain: (index < Mpv.equalizerGains.length) ? Mpv.equalizerGains[index] : 0.0

                                // dB label at top
                                Text {
                                    anchors.top: parent.top
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: (currentGain > 0 ? "+" : "") + currentGain.toFixed(0)
                                    color: currentGain !== 0 ? "#FFFFFF" : Theme.textMuted
                                    font.family: Theme.fontDigits
                                    font.pixelSize: 10
                                    font.bold: currentGain !== 0
                                }

                                // Vertical Track Area
                                Item {
                                    id: vTrackArea
                                    anchors.top: parent.top
                                    anchors.topMargin: 18
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 20
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: 22

                                    // Rail
                                    Rectangle {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        width: 3
                                        radius: 1.5
                                        color: "#222736"

                                        // 0 dB center tick
                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: 8
                                            height: 1
                                            color: "#444B60"
                                        }

                                        // Fill towards gain
                                        Rectangle {
                                            property real centerPos: parent.height / 2
                                            property real targetPos: centerPos - (currentGain / 12.0) * centerPos
                                            y: Math.min(centerPos, targetPos)
                                            height: Math.abs(targetPos - centerPos)
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            width: 3
                                            radius: 1.5
                                            color: "#3A7BD5"
                                        }
                                    }

                                    // Thumb
                                    Rectangle {
                                        y: (parent.height / 2) - (currentGain / 12.0) * (parent.height / 2) - height / 2
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        width: 14
                                        height: 14
                                        radius: 7
                                        color: bandMouse.containsMouse ? "#FFFFFF" : "#D2D6E4"
                                        border.color: "#3A7BD5"
                                        border.width: 1
                                    }

                                    MouseArea {
                                        id: bandMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        enabled: Mpv.equalizerEnabled

                                        function updateGain(my) {
                                            var ratio = (height / 2 - my) / (height / 2);
                                            ratio = Math.max(-1.0, Math.min(1.0, ratio));
                                            var gain = Math.round(ratio * 12.0 * 2) / 2.0; // 0.5dB step
                                            Mpv.setEqualizerBandGain(index, gain);
                                            Config.setEqualizerGains(Mpv.equalizerGains);
                                        }

                                        onPressed: (mouse) => updateGain(mouse.y)
                                        onPositionChanged: (mouse) => {
                                            if (pressed) updateGain(mouse.y);
                                        }
                                        onDoubleClicked: {
                                            Mpv.setEqualizerBandGain(index, 0.0);
                                            Config.setEqualizerGains(Mpv.equalizerGains);
                                        }
                                    }
                                }

                                // Frequency label at bottom
                                Text {
                                    anchors.bottom: parent.bottom
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: bandsContainer.freqLabels[index]
                                    color: Theme.textSecondary
                                    font.family: Theme.fontDigits
                                    font.pixelSize: 10
                                }
                            }
                        }
                    }
                }

                // Reset Equalizer Button
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 180
                    height: 28
                    radius: Theme.radiusSmall
                    color: resetEqMouse.containsMouse ? "#2A3348" : "#1C202C"
                    border.color: "#2C3244"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "Reset to Flat (0 dB)"
                        color: resetEqMouse.containsMouse ? "#FFFFFF" : Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                    }

                    MouseArea {
                        id: resetEqMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            Mpv.resetEqualizer();
                            Config.setEqualizerPreset("Flat");
                            Config.setEqualizerGains(Mpv.equalizerGains);
                        }
                    }
                }
            }

            // ==========================================
            // TAB 2: SUBTITLES SETTINGS
            // ==========================================
            Item {
                id: subTab
                anchors.fill: parent
                visible: panel.currentTab === 2

                Column {
                    anchors.fill: parent
                    spacing: 12

                    // Subtitle Visibility Toggle
                    Row {
                        spacing: 12

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 36
                            height: 18
                            radius: 9
                            color: Mpv.subtitlesVisible ? "#3A7BD5" : "#222736"

                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                x: Mpv.subtitlesVisible ? parent.width - width - 2 : 2
                                width: 14
                                height: 14
                                radius: 7
                                color: "#FFFFFF"
                                Behavior on x { NumberAnimation { duration: Theme.animFast } }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    var v = !Mpv.subtitlesVisible;
                                    Mpv.setSubtitlesVisible(v);
                                }
                            }
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: Mpv.subtitlesVisible ? "Subtitles Visible (V)" : "Subtitles Hidden (V)"
                            color: Mpv.subtitlesVisible ? "#FFFFFF" : Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontNormal
                            font.bold: true
                        }
                    }

                    Rectangle { width: parent.width; height: 1; color: "#202534" }

                    // Subtitle Sync / Delay
                    Column {
                        width: parent.width
                        spacing: 6

                        Item {
                            width: parent.width
                            height: 20
                            Text {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                text: "Subtitle Sync / Delay:"
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontNormal
                            }
                            Text {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                text: (Mpv.subtitleDelay > 0 ? "+" : "") + Mpv.subtitleDelay.toFixed(2) + "s"
                                color: Mpv.subtitleDelay !== 0.0 ? "#FFFFFF" : Theme.textMuted
                                font.family: Theme.fontDigits
                                font.pixelSize: Theme.fontNormal
                                font.bold: true
                            }
                        }

                        Row {
                            spacing: 8
                            Repeater {
                                model: [
                                    { text: "−0.5s", d: -0.5 },
                                    { text: "−0.1s", d: -0.1 },
                                    { text: "0.0s (Reset)", d: 0.0, reset: true },
                                    { text: "+0.1s", d: 0.1 },
                                    { text: "+0.5s", d: 0.5 }
                                ]

                                delegate: Rectangle {
                                    width: 80
                                    height: 26
                                    radius: Theme.radiusSmall
                                    color: btnMouse.containsMouse ? "#2A3348" : "#1A1D27"
                                    border.color: "#282E3E"
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.text
                                        color: modelData.reset ? "#4A90E2" : (btnMouse.containsMouse ? "#FFFFFF" : Theme.textSecondary)
                                        font.family: Theme.fontDigits
                                        font.pixelSize: Theme.fontSmall
                                        font.bold: modelData.reset
                                    }

                                    MouseArea {
                                        id: btnMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: {
                                            if (modelData.reset) {
                                                Mpv.setSubtitleDelay(0.0);
                                                Config.setSubtitleDelay(0.0);
                                            } else {
                                                var nd = Mpv.subtitleDelay + modelData.d;
                                                Mpv.setSubtitleDelay(nd);
                                                Config.setSubtitleDelay(nd);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Rectangle { width: parent.width; height: 1; color: "#202534" }

                    // Subtitle Font Size
                    Row {
                        width: parent.width
                        spacing: 12

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 80
                            text: "Font Size:"
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontNormal
                        }

                        Item {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 260
                            height: 24

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                height: 4
                                radius: 2
                                color: "#222736"

                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    width: parent.width * Math.max(0.0, Math.min(1.0, (Mpv.subtitleFontSize - 18) / (72 - 18)))
                                    radius: 2
                                    color: "#3A7BD5"
                                }
                            }

                            Rectangle {
                                x: parent.width * Math.max(0.0, Math.min(1.0, (Mpv.subtitleFontSize - 18) / (72 - 18))) - width / 2
                                anchors.verticalCenter: parent.verticalCenter
                                width: 14
                                height: 14
                                radius: 7
                                color: fMouse.containsMouse ? "#FFFFFF" : "#E0E3EC"
                            }

                            MouseArea {
                                id: fMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                function updateSize(mx) {
                                    var r = Math.max(0.0, Math.min(1.0, mx / width));
                                    var sz = Math.round(18 + r * (72 - 18));
                                    Mpv.setSubtitleFontSize(sz);
                                    Config.setSubtitleFontSize(sz);
                                }
                                onPressed: (mouse) => updateSize(mouse.x)
                                onPositionChanged: (mouse) => { if (pressed) updateSize(mouse.x); }
                            }
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: Mpv.subtitleFontSize.toFixed(0) + "px"
                            color: "#FFFFFF"
                            font.family: Theme.fontDigits
                            font.pixelSize: Theme.fontNormal
                        }
                    }

                    // Subtitle Position
                    Row {
                        width: parent.width
                        spacing: 12

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 80
                            text: "Position:"
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontNormal
                        }

                        Item {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 260
                            height: 24

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                height: 4
                                radius: 2
                                color: "#222736"

                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    width: parent.width * Math.max(0.0, Math.min(1.0, Mpv.subtitlePosition / 120.0))
                                    radius: 2
                                    color: "#3A7BD5"
                                }
                            }

                            Rectangle {
                                x: parent.width * Math.max(0.0, Math.min(1.0, Mpv.subtitlePosition / 120.0)) - width / 2
                                anchors.verticalCenter: parent.verticalCenter
                                width: 14
                                height: 14
                                radius: 7
                                color: posMouse.containsMouse ? "#FFFFFF" : "#E0E3EC"
                            }

                            MouseArea {
                                id: posMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                function updatePos(mx) {
                                    var r = Math.max(0.0, Math.min(1.0, mx / width));
                                    var p = Math.round(r * 120.0);
                                    Mpv.setSubtitlePosition(p);
                                    Config.setSubtitlePosition(p);
                                }
                                onPressed: (mouse) => updatePos(mouse.x)
                                onPositionChanged: (mouse) => { if (pressed) updatePos(mouse.x); }
                            }
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: Mpv.subtitlePosition.toFixed(0) + "%"
                            color: "#FFFFFF"
                            font.family: Theme.fontDigits
                            font.pixelSize: Theme.fontNormal
                        }
                    }
                }
            }

            // ==========================================
            // TAB 3: PLAYBACK & CONTROLS
            // ==========================================
            Item {
                id: playTab
                anchors.fill: parent
                visible: panel.currentTab === 3

                Column {
                    anchors.fill: parent
                    spacing: 12

                    // Playback Speed
                    Row {
                        width: parent.width
                        spacing: 10

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 60
                            text: "Speed:"
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontNormal
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 50
                            text: Mpv.speed.toFixed(2) + "×"
                            color: "#FFFFFF"
                            font.family: Theme.fontDigits
                            font.pixelSize: Theme.fontNormal
                            font.bold: true
                        }

                        Repeater {
                            model: [
                                { label: "0.5×", s: 0.5 },
                                { label: "0.75×", s: 0.75 },
                                { label: "1.0×", s: 1.0 },
                                { label: "1.25×", s: 1.25 },
                                { label: "1.5×", s: 1.5 },
                                { label: "2.0×", s: 2.0 }
                            ]

                            delegate: Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 50
                                height: 26
                                radius: Theme.radiusSmall
                                property bool isCurrent: Math.abs(Mpv.speed - modelData.s) < 0.01
                                color: isCurrent ? "#2A3348" : (spMouse.containsMouse ? Theme.bgGlassHover : "#181B26")
                                border.color: isCurrent ? "#4A90E2" : "#242938"
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: isCurrent ? "#FFFFFF" : Theme.textSecondary
                                    font.family: Theme.fontDigits
                                    font.pixelSize: Theme.fontSmall
                                    font.bold: isCurrent
                                }

                                MouseArea {
                                    id: spMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: Mpv.setSpeed(modelData.s)
                                }
                            }
                        }
                    }

                    Rectangle { width: parent.width; height: 1; color: "#202534" }

                    // A-B Repeat Loop
                    Column {
                        width: parent.width
                        spacing: 6

                        Row {
                            spacing: 8
                            Text {
                                text: "A-B Repeat Loop:"
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontNormal
                            }

                            Text {
                                text: Mpv.abLoopActive ? "Active" : "Inactive"
                                color: Mpv.abLoopActive ? "#00E5FF" : Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontNormal
                                font.bold: true
                            }
                        }

                        Row {
                            spacing: 8

                            Rectangle {
                                width: 120
                                height: 28
                                radius: Theme.radiusSmall
                                color: setAMouse.containsMouse ? "#2A3348" : "#181B26"
                                border.color: Mpv.abLoopA >= 0 ? "#00E5FF" : "#282E3E"
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: Mpv.abLoopA >= 0 ? "A: " + Mpv.formatTime(Mpv.abLoopA) : "Set A ( [ )"
                                    color: Mpv.abLoopA >= 0 ? "#00E5FF" : Theme.textSecondary
                                    font.family: Theme.fontDigits
                                    font.pixelSize: Theme.fontSmall
                                }

                                MouseArea {
                                    id: setAMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: Mpv.setAbLoopA()
                                }
                            }

                            Rectangle {
                                width: 120
                                height: 28
                                radius: Theme.radiusSmall
                                color: setBMouse.containsMouse ? "#2A3348" : "#181B26"
                                border.color: Mpv.abLoopB >= 0 ? "#00E5FF" : "#282E3E"
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: Mpv.abLoopB >= 0 ? "B: " + Mpv.formatTime(Mpv.abLoopB) : "Set B ( ] )"
                                    color: Mpv.abLoopB >= 0 ? "#00E5FF" : Theme.textSecondary
                                    font.family: Theme.fontDigits
                                    font.pixelSize: Theme.fontSmall
                                }

                                MouseArea {
                                    id: setBMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: Mpv.setAbLoopB()
                                }
                            }

                            Rectangle {
                                width: 100
                                height: 28
                                radius: Theme.radiusSmall
                                color: clearLoopMouse.containsMouse ? "#2A3348" : "#181B26"
                                border.color: "#282E3E"
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: "Clear ( \\ )"
                                    color: clearLoopMouse.containsMouse ? "#FFFFFF" : Theme.textSecondary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSmall
                                }

                                MouseArea {
                                    id: clearLoopMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: Mpv.clearAbLoop()
                                }
                            }
                        }
                    }

                    Rectangle { width: parent.width; height: 1; color: "#202534" }

                    // Video Aspect Ratio
                    Row {
                        width: parent.width
                        spacing: 8

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 60
                            text: "Aspect:"
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontNormal
                        }

                        Repeater {
                            model: ["default", "16:9", "4:3", "2.35:1"]

                            delegate: Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 80
                                height: 26
                                radius: Theme.radiusSmall
                                color: Mpv.videoAspect === modelData ? "#2A3348" : (aspMouse.containsMouse ? Theme.bgGlassHover : "#181B26")
                                border.color: Mpv.videoAspect === modelData ? "#4A90E2" : "#242938"
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData === "default" ? "Auto" : modelData
                                    color: Mpv.videoAspect === modelData ? "#FFFFFF" : Theme.textSecondary
                                    font.family: Theme.fontDigits
                                    font.pixelSize: Theme.fontSmall
                                    font.bold: Mpv.videoAspect === modelData
                                }

                                MouseArea {
                                    id: aspMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: {
                                        Mpv.setVideoAspect(modelData);
                                        Config.setVideoAspect(modelData);
                                    }
                                }
                            }
                        }
                    }

                    // Video Rotate
                    Row {
                        width: parent.width
                        spacing: 8

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 60
                            text: "Rotate:"
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontNormal
                        }

                        Repeater {
                            model: [0, 90, 180, 270]

                            delegate: Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 80
                                height: 26
                                radius: Theme.radiusSmall
                                color: Mpv.videoRotate === modelData ? "#2A3348" : (rotMouse.containsMouse ? Theme.bgGlassHover : "#181B26")
                                border.color: Mpv.videoRotate === modelData ? "#4A90E2" : "#242938"
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData + "°"
                                    color: Mpv.videoRotate === modelData ? "#FFFFFF" : Theme.textSecondary
                                    font.family: Theme.fontDigits
                                    font.pixelSize: Theme.fontSmall
                                    font.bold: Mpv.videoRotate === modelData
                                }

                                MouseArea {
                                    id: rotMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: Mpv.setVideoRotate(modelData)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
