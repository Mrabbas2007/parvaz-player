# AGENTS.md — قواعد کار روی Parvaz

> این فایل را هر نشست AI اول بخوان. اگر با نقشه تضاد داشت، `ROADMAP.md` اولویت دارد.

## معماری (تغییرناپذیر)

- موتور: **libmpv** با `vo=libmpv` + `MPV_RENDER_API_TYPE_OPENGL`
- پوسته: C++20 + Qt6/QML
- تصویر: `QQuickFramebufferObject` → `MpvFboItem` → `MpvRenderer`
- `mpv_handle` فقط در **GUI thread** لمس می‌شود (`MpvObject`)
- `mpv_render_context` فقط در **render thread** لمس می‌شود (`MpvRenderer`)
- بک‌اند RHI باید OpenGL باشد (`QQuickWindow::setGraphicsApi`)

## قوانین سخت

1. **هیچ فریمی کپی نمی‌شود** در مسیر داغ. بدون `glReadPixels`، بدون `QImage`، بدون `toImage()`.
2. **هیچ فراخوانی mpv از thread اشتباه.** اگر مطمئن نیستی، متوقف شو و بپرس.
3. **نام تابع و property را حدس نزن.** مرجع: `mpv/client.h`، `mpv/render_gl.h`، `docs/mpv-properties.md`.
4. **هر تغییر باید build شود و اجرا شود.** «باید کار کند» پاسخ قابل قبول نیست.
5. **منطق در C++، QML فقط چیدمان.** JS پیچیده در QML ممنوع.
6. **`ninja -C build check` باید سبز بماند.** اگر شکست، اول همان را درست کن.
7. **یک commit به ازای هر نقطه‌ی سبز.**
8. **دیباگ با `fprintf(stderr)`، نه `qDebug`.** بیلد Qt فدورا با `QT_NO_DEBUG_OUTPUT` است و `qInfo`/`console.log` ساکت می‌مانند.

## دستورها

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
ninja -C build
./build/parvaz file.mkv
ninja -C build check                 # build + کلیپ تست + smoke واقعی
./build/parvaz --bench 8 file.mkv      # frames / pos / RSS / dropped / hwdec
```

## دام‌های واقعیِ برخوردکرده (همه تأییدشده روی همین پروژه)

| # | نشانه | علت / راه‌حل |
|---|---|---|
| 1 | `'mpv_render_param' was not declared` | render API در `<mpv/render_gl.h>` است، نه `client.h` |
| 2 | `'class MpvFboItem' has no member named 'framebufferObject'` | در Qt6 این تابع عضو **protected خودِ `Renderer`** است. در `synchronize()` با `this->framebufferObject()` بگیر |
| 3 | `invalid use of incomplete type QOpenGLFramebufferObject` | `#include <QOpenGLFramebufferObject>` + کامپوننت `Qt6::OpenGL` |
| 4 | `Non-C locale detected. This is not supported.` | `std::setlocale(LC_NUMERIC, "C")` **بعد از** ساخت `QGuiApplication` (Qt لوکیل را ریست می‌کند) |
| 5 | `start-file` بعد `end-file` بدون `file-loaded` | فایل قبل از وجود render context لود شده؛ mpv نمی‌تواند VO بسازد. لود را تا `setRenderReady()` عقب بینداز |
| 6 | **یک فریم رندر شد و `time-pos` روی ۰ قفل شد** | `MPV_RENDER_PARAM_ADVANCED_CONTROL=1` بدون `mpv_render_context_report_swap()`. فعلاً خاموش بماند |
| 7 | تصویر فریز | `mpv_render_context_set_update_callback` ست نشده یا `update()` روی thread اشتباه |
| 8 | `hwdec=no` روی AMD | بسته‌ی `mesa-va-drivers-freeworld` (RPM Fusion) نصب نیست |
| 9 | تصویر وارونه | `MPV_RENDER_PARAM_FLIP_Y` را ۰ کن |
| 10 | کرش در خروج | اول `mpv_render_context_free` بعد `mpv_terminate_destroy` |
| 11 | خروج `parvaz.exe` با `0xC0000135` (STATUS_DLL_NOT_FOUND) قبل از اجرای main، بدون هیچ لاگی | بیلد shinchiro `libmpv-2.dll` استاتیک `vulkan-1.dll` را import می‌کند؛ ویندوز خام و رانر CI لودر Vulkan ندارند. فیکس: `vulkan-1.dll` رسمی x64 کنار اکسه (step «Deploy smoke runtime DLLs» در ci.yml). مشکل دوم بعدی هم GL است: `--smoke` بک‌اند OpenGL می‌خواهد و رانر فقط GDI GL 1.1 دارد → `opengl32.dll` مِسا (llvmpipe) کنار اکسه |

## دیباگ

| مشکل | ابزار |
|---|---|
| صفحه سیاه | `QSG_INFO=1 QT_LOGGING_RULES="qt.quick.*=true" ./build/parvaz f.mkv` |
| پیام‌های mpv | در کد: `mpv_set_option_string(h,"msg-level","all=v")` |
| Wayland | `WAYLAND_DEBUG=1` |
| Mesa/EGL | `MESA_DEBUG=1` |
| hwdec واقعی | `--bench` (property `hwdec-current`) |
| افت فریم | `--bench` (property `frame-drop-count`) |

## تعریف «تمام‌شده» برای فاز ۰

1080p60، صفر dropped frame، CPU < 10٪، RSS < 200MB، resize بدون artifact، خروج تمیز.
وضعیت فعلی: پخش و رندر تأیید شد؛ hwdec و RSS هنوز نه (بخش `bench.md`).
