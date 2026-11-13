pragma Singleton
import QtQuick

QtObject {
    // ---- سطوح گرافیتی و شیشه‌ای استودیویی (Cinematic Studio Monochrome) ----
    readonly property color bgBase: "#000000"
    readonly property color bgSurface: "#0C0D11"
    readonly property color bgGlass: "#CC0C0D12"           // مشکی گرافیتی نیمه‌شفاف
    readonly property color bgGlassLight: "#181A22"
    readonly property color bgGlassHover: "#1FFFFFFF"      // هاور بسیار ملایم ۱۲٪
    readonly property color bgGlassPressed: "#33FFFFFF"
    readonly property color bgTooltip: "#F00C0D12"

    // ---- حاشیه‌های ظریف و محو (Hairline Studio Highlights) ----
    readonly property color borderGlass: "#1AFFFFFF"       // هایلایت لبه ۱۰٪ سفید
    readonly property color borderGlassSubtle: "#0DFFFFFF" // ۵٪ سفید
    readonly property color borderTooltip: "#26FFFFFF"

    // ---- پالت مونوکروم و باوقار (بدون رنگ‌های نئونی هوش مصنوعی) ----
    readonly property color textPrimary: "#FFFFFF"
    readonly property color textSecondary: "#B0B3BD"
    readonly property color textMuted: "#666B7A"
    readonly property color accent: "#FFFFFF"              // سفید خالص برای هایلایت‌های اصلی
    readonly property color accentSubtle: "#1AFFFFFF"
    readonly property color timelinePlayed: "#FFFFFF"      // خط تایم‌لاین سفید باوقار
    readonly property color timelineRail: "#26FFFFFF"      // ریل خاکستری مات
    readonly property color statusGreen: "#10B981"         // نقطه سبز مینیمال HW

    // ---- تایپوگرافی (Vazirmatn با Fallback) ----
    readonly property string fontFamily: "Vazirmatn"
    readonly property string fontFallback: "Segoe UI, Inter, sans-serif"
    readonly property int fontSmall: 11
    readonly property int fontNormal: 12
    readonly property int fontMedium: 13
    readonly property int fontLarge: 15
    readonly property int fontTitle: 15

    // ---- ابعاد و شعاع‌ها ----
    readonly property real radiusLarge: 12
    readonly property real radiusMedium: 6
    readonly property real radiusSmall: 4
    readonly property real controlBarHeight: 80
    readonly property real titleBarHeight: 44

    // ---- زمان‌بندی انیمیشن‌ها ----
    readonly property int animFast: 120
    readonly property int animNormal: 200
    readonly property int animSlow: 300
    readonly property int autoHideTimeout: 2200
}
