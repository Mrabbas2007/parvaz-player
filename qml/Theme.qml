pragma Singleton
import QtQuick

QtObject {
    // ---- سطوح و پس‌زمینه‌ها (Dark Charcoal & Glass) ----
    readonly property color bgBase: "#0D0E12"
    readonly property color bgSurface: "#13151B"
    readonly property color bgGlass: "#E0151821"           // شیشه اصلی داک و پنل‌ها
    readonly property color bgGlassLight: "#EB20242E"      // پنل‌های هایلایت‌شده
    readonly property color bgGlassHover: "#26FFFFFF"      // پس‌زمینه هاور دکمه‌ها
    readonly property color bgGlassPressed: "#3DFFFFFF"    // پس‌زمینه فشرده شدن
    readonly property color bgTooltip: "#F212141A"         // پس‌زمینه کپسول تولتیپ

    // ---- حاشیه‌ها و لبه‌های شیشه‌ای (1px Border Highlights) ----
    readonly property color borderGlass: "#26FFFFFF"       // هایلایت لبه ۱۵٪ سفید
    readonly property color borderGlassSubtle: "#14FFFFFF" // هایلایت ملایم ۸٪ سفید
    readonly property color borderGlassActive: "#4038BDF8" // لبه فعال/انتخاب‌شده
    readonly property color borderTooltip: "#33FFFFFF"

    // ---- رنگ‌های تاکیدی (Accents) ----
    readonly property color accent: "#38BDF8"             // آبی فیروزه‌ای مدرن (Sky Blue)
    readonly property color accentHover: "#0EA5E9"
    readonly property color accentGlow: "#3338BDF8"
    readonly property color accentGradientStart: "#38BDF8"
    readonly property color accentGradientEnd: "#0284C7"

    // ---- رنگ‌های وضعیت و متن ----
    readonly property color textPrimary: "#F8FAFC"
    readonly property color textSecondary: "#94A3B8"
    readonly property color textMuted: "#64748B"
    readonly property color statusGreen: "#34D399"
    readonly property color statusRed: "#F87171"

    // ---- تایپوگرافی (Vazirmatn با Fallback) ----
    readonly property string fontFamily: "Vazirmatn"
    readonly property string fontFallback: "Segoe UI, Inter, sans-serif"
    readonly property int fontSmall: 11
    readonly property int fontNormal: 13
    readonly property int fontMedium: 14
    readonly property int fontLarge: 16
    readonly property int fontTitle: 16

    // ---- ابعاد و شعاع‌ها ----
    readonly property real radiusPill: 999
    readonly property real radiusLarge: 12
    readonly property real radiusMedium: 8
    readonly property real radiusSmall: 4
    readonly property real controlBarHeight: 76
    readonly property real titleBarHeight: 44

    // ---- زمان‌بندی انیمیشن‌ها ----
    readonly property int animFast: 120
    readonly property int animNormal: 200
    readonly property int animSlow: 350
    readonly property int autoHideTimeout: 2200
}
