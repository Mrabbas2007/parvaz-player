pragma Singleton
import QtQuick
import Parvaz 1.0

QtObject {
    id: themeRoot

    // ---- Active Theme State ----
    property string currentTheme: (typeof Config !== "undefined" && Config.theme) ? Config.theme : "midnight"

    function setTheme(name) {
        if (!name) return;
        currentTheme = name;
        if (typeof Config !== "undefined")
            Config.setTheme(name);
    }

    // Available Themes Metadata for UI selection
    readonly property var availableThemes: [
        { id: "midnight", name: "Midnight Studio", accent: "#FFFFFF", surface: "#0C0D11" },
        { id: "oled", name: "OLED Pure Black", accent: "#FFFFFF", surface: "#000000" },
        { id: "potplayer", name: "PotPlayer Classic", accent: "#F5A623", surface: "#1C1E24" },
        { id: "cyber", name: "Cyber Neon", accent: "#00F0FF", surface: "#0B0E17" }
    ]

    // ---- Studio Glass & Graphite Surfaces ----
    readonly property color bgBase: {
        if (currentTheme === "oled") return "#000000";
        if (currentTheme === "potplayer") return "#121316";
        if (currentTheme === "cyber") return "#06080F";
        return "#000000";
    }
    readonly property color bgSurface: {
        if (currentTheme === "oled") return "#000000";
        if (currentTheme === "potplayer") return "#1C1E24";
        if (currentTheme === "cyber") return "#0B0E17";
        return "#0C0D11";
    }
    readonly property color bgGlass: {
        if (currentTheme === "oled") return "#E6000000";
        if (currentTheme === "potplayer") return "#E01A1C22";
        if (currentTheme === "cyber") return "#D90B0E18";
        return "#CC0C0D12";
    }
    readonly property color bgGlassLight: {
        if (currentTheme === "oled") return "#0F0F0F";
        if (currentTheme === "potplayer") return "#262932";
        if (currentTheme === "cyber") return "#141A29";
        return "#181A22";
    }
    readonly property color bgGlassHover: "#1FFFFFFF"
    readonly property color bgGlassPressed: "#33FFFFFF"
    readonly property color bgTooltip: {
        if (currentTheme === "oled") return "#F0000000";
        if (currentTheme === "potplayer") return "#F01A1C22";
        if (currentTheme === "cyber") return "#F00B0E18";
        return "#F00C0D12";
    }

    // ---- Subtle Glass Borders ----
    readonly property color borderGlass: {
        if (currentTheme === "oled") return "#222222";
        if (currentTheme === "potplayer") return "#363B48";
        if (currentTheme === "cyber") return "#2A3650";
        return "#1AFFFFFF";
    }
    readonly property color borderGlassSubtle: {
        if (currentTheme === "oled") return "#141414";
        if (currentTheme === "potplayer") return "#252932";
        if (currentTheme === "cyber") return "#182030";
        return "#0DFFFFFF";
    }
    readonly property color borderTooltip: {
        if (currentTheme === "oled") return "#282828";
        if (currentTheme === "potplayer") return "#3F4554";
        if (currentTheme === "cyber") return "#354466";
        return "#26FFFFFF";
    }

    // ---- Color Palette & Typography ----
    readonly property color textPrimary: "#FFFFFF"
    readonly property color textSecondary: {
        if (currentTheme === "oled") return "#9E9E9E";
        if (currentTheme === "potplayer") return "#B4B9C7";
        if (currentTheme === "cyber") return "#8E9EB8";
        return "#B0B3BD";
    }
    readonly property color textMuted: {
        if (currentTheme === "oled") return "#555555";
        if (currentTheme === "potplayer") return "#6C7387";
        if (currentTheme === "cyber") return "#54647F";
        return "#666B7A";
    }
    readonly property color accent: {
        if (currentTheme === "potplayer") return "#F5A623";
        if (currentTheme === "cyber") return "#00F0FF";
        return "#FFFFFF";
    }
    readonly property color accentColor: {
        if (currentTheme === "oled") return "#E0E0E0";
        if (currentTheme === "potplayer") return "#F5A623";
        if (currentTheme === "cyber") return "#00F0FF";
        return "#4A90E2";
    }
    readonly property color accentSubtle: {
        if (currentTheme === "potplayer") return "#26F5A623";
        if (currentTheme === "cyber") return "#2600F0FF";
        return "#1AFFFFFF";
    }
    readonly property color timelinePlayed: {
        if (currentTheme === "potplayer") return "#F5A623";
        if (currentTheme === "cyber") return "#00F0FF";
        return "#FFFFFF";
    }
    readonly property color timelineRail: "#26FFFFFF"
    readonly property color statusGreen: "#10B981"

    // ---- Studio Dual Typography ----
    readonly property string fontFamily: "Inter, Segoe UI, -apple-system, sans-serif, Vazirmatn"
    readonly property string fontDigits: "JetBrains Mono, monospace, Vazirmatn"

    readonly property int fontSmall: 11
    readonly property int fontNormal: 12
    readonly property int fontMedium: 13
    readonly property int fontLarge: 15
    readonly property int fontTitle: 15

    // ---- Dimensions & Radii ----
    readonly property real radiusLarge: 12
    readonly property real radiusMedium: 6
    readonly property real radiusSmall: 4
    readonly property real controlBarHeight: 80
    readonly property real titleBarHeight: 44

    // ---- Animation Timings ----
    readonly property int animFast: 120
    readonly property int animNormal: 200
    readonly property int animSlow: 300
    readonly property int autoHideTimeout: 2200
}
