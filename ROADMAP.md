# Parvaz — نقشه‌ی نهایی

> **نام پروژه:** Parvaz Player (پرواز) — «PotPlayer» علامت تجاری Kakao است و قابل استفاده نیست. فقط در توضیحات بنویس «PotPlayer-style» و گاه‌به‌گاه اسم را در TMview و GitHub چک کن.
>
> **App ID:** `io.github.<username>.ParvazPlayer` · **مخزن:** `parvaz-player` · **باینری:** `parvaz`

---

## ۰. تصمیمات قطعی

| موضوع | تصمیم | دلیل |
|---|---|---|
| موتور | **libmpv** + `vo=gpu-next` (libplacebo) | دیکودر/دموکس/سینک/زیرنویس/HDR آماده و بهترین روی لینوکس |
| زبان پوسته | **C++20** | تنها زبانی که برای هر درد لینوکسی‌اش نمونه‌ی حل‌شده‌ی واقعی وجود دارد |
| UI | **Qt6 / QML** (کامپوننت‌های سفارشی، نه Quick Controls) | چگال، اسکین‌پذیر، HiDPI-native، Wayland-native |
| مسیر تصویر | **`QQuickFramebufferObject` + `MPV_RENDER_API_TYPE_OPENGL`** | libmpv فقط OpenGL و SW را در render API عمومی دارد |
| بک‌اند گرافیک Qt | **اجبار به OpenGL RHI** | Vulkan RHI با QQuickFBO + mpv سازگار نیست |
| تنظیمات | TOML (`toml++`) | خوانا و قابل ویرایش دستی، مناسب power-user |
| دیتابیس | `QtSql` + SQLite | بدون وابستگی اضافه |
| D-Bus / MPRIS | `QDBusAbstractAdaptor` | بومی Qt، صفر وابستگی |
| بسته‌بندی | **Flatpak اول** | تنها راه واقعی «just works» + کدک کامل |
| لایسنس | **GPLv3** (اجباری) | libmpv در بیلد توزیع‌ها GPLv2+ است؛ لینک کردن، پروژه را GPL می‌کند |

### دو قانون تغییرناپذیر

1. **هیچ فریمی از حافظه‌ی C++ کپی نمی‌شود.** رندر مستقیم در FBO. یک `glReadPixels` یا `QImage` در مسیر داغ = مرگ performance.
2. **همه‌ی فراخوانی‌های render API فقط در render thread و با GL context فعال.** بقیه‌ی فراخوانی‌های mpv از GUI thread.

---

## ۱. معماری و مدل تردی

```
┌──────────────────── GUI Thread ─────────────────────┐
│  MpvObject (QObject)                                │
│    ├─ Q_PROPERTY: position, duration, pause, volume │
│    ├─ Q_INVOKABLE: play, seek, setSpeed, ...        │
│    └─ signal: positionChanged, fileLoaded, ...      │
│  QML Engine (Main.qml, ControlBar, Playlist, OSD)   │
│  Library / Config / MPRIS (QtSql, toml++, QDBus)    │
└───────────────┬─────────────────────────────────────┘
                │ mpv_wakeup_callback → QueuedConnection
┌───────────────▼────────── Render Thread ────────────┐
│  MpvFboRenderer : QQuickFramebufferObject::Renderer │
│    render()      → mpv_render_context_render()      │
│    synchronize() → mpv_render_context_update()      │
│  GL context فعال، صفر کپی، FBO مشترک با Qt          │
└───────────────┬─────────────────────────────────────┘
                │
        libmpv (gpu-next / libplacebo / hwdec)
```

### نکات حیاتی پیاده‌سازی

| قطعه | جزئیات |
|---|---|
| `mpv_set_wakeup_callback` | **قبل از** `mpv_initialize` ست شود؛ داخلش `QMetaObject::invokeMethod(obj, ..., Qt::QueuedConnection)` |
| حلقه‌ی رویداد | در GUI thread یک `QTimer(0)` یا `QSocketNotifier` که `mpv_wait_event(h, 0)` را تا `MPV_EVENT_NONE` می‌خواند |
| `mpv_render_context_set_update_callback` | داخلش `renderer->update()` با `Qt::QueuedConnection` — **بدون این، تصویر فریز است** |
| `MPV_RENDER_PARAM_OPENGL_INIT_PARAMS` | `get_proc_address` از `QOpenGLContext::currentContext()->getProcAddress` |
| `MPV_RENDER_PARAM_OPENGL_FBO` | `framebufferObject()` + `width`/`height` + `internal_format` |
| `MPV_RENDER_PARAM_ADVANCED_CONTROL = 1` | برای frame timing بهتر؛ بعد از رندر `mpv_render_context_report_swap()` |
| بک‌اند RHI | `QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL)` **قبل از** ساخت پنجره |
| `vo` | `vo=libmpv` (نه `gpu`) وقتی از render API استفاده می‌کنی |
| hwdec | `hwdec=auto-safe`؛ روی NVIDIA/Wayland اگر مشکل بود → `hwdec=vaapi` یا `nvdec` |

---

## ۲. ساختار مخزن

```
parvaz/
├── CMakeLists.txt
├── AGENTS.md                     # قواعد AI + معماری + invariants
├── ROADMAP.md
├── bench.md                      # اعداد واقعی هر فاز
├── bench.sh
├── src/
│   ├── main.cpp                  # setGraphicsApi, ثبت تایپ‌ها
│   ├── mpv/
│   │   ├── MpvObject.h/.cpp      # QObject wrapper + event loop
│   │   ├── MpvFboItem.h/.cpp     # QQuickFramebufferObject
│   │   └── MpvFboRenderer.h/.cpp # Renderer + render API
│   ├── ui/
│   │   ├── MainController.h/.cpp
│   │   └── ThemeProvider.h/.cpp
│   ├── library/                  # SQLite، watch folders، thumbnails
│   ├── subs/                     # charset detect، search، sync
│   ├── config/                   # TOML
│   └── dbus/                     # MPRIS adaptor
├── qml/
│   ├── Main.qml
│   ├── components/               # Button, Slider, IconButton (سفارشی)
│   ├── VideoView.qml             # MpvFboItem
│   ├── ControlBar.qml
│   ├── Osd.qml
│   ├── SeekBar.qml               # + thumbnail preview
│   ├── PlaylistPanel.qml
│   ├── Menu/                     # منوی راست‌کلیک
│   └── Dialogs/
├── data/                         # آیکون، فونت، preset کیبورد
├── docs/
│   ├── mpv-properties.md         # استخراج‌شده از mpv --list-properties
│   └── mpv-commands.md
└── flatpak/
    └── io.github.<user>.ParvazPlayer.yml
```

### پیش‌نیازها (Fedora)

```bash
sudo dnf install gcc-c++ cmake ninja-build mpv-libs-devel \
  qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtbase-private-devel \
  mesa-libGL-devel libplacebo-devel sqlite-devel
```

---

## ۳. فاز ۰ — Spike (۲ تا ۳ روز) ⚠️ دروازه‌ی پروژه

هدف: **اثبات مسیر رندر.** تا سبز نشدن این فاز، هیچ فیچری اضافه نکن.

### وظایف دقیق

1. `CMakeLists.txt` با `find_package(Qt6)` + `pkg_check_modules(MPV REQUIRED mpv)`
2. `main.cpp`: `QQuickWindow::setGraphicsApi(OpenGL)` → `QQmlApplicationEngine` → `qmlRegisterType<MpvFboItem>("Parvaz", 1, 0, "MpvFboItem")`
3. `MpvObject`: ساخت handle، `mpv_set_option_string(h,"vo","libmpv")`، `hwdec=auto-safe`، `set_wakeup_callback`، `initialize`، حلقه‌ی رویداد
4. `MpvFboRenderer`: `synchronize()` → init render context؛ `render()` → `mpv_render_context_render` در FBO
5. `Main.qml`: یک `MpvFboItem` تمام‌صفحه + `Shortcut` برای Space/Escape
6. اجرا با `--wid` **ممنوع** (روی Wayland کار نمی‌کند)

### معیار قبولی

- [x] ۱۰۸۰p۶۰ H.264 پخش realtime (۴۷۲/۴۸۰ فریم در ۸ ثانیه)
- [ ] صفر dropped frame — فعلاً ۱۴ (دیکود نرم‌افزاری)
- [ ] CPU زیر ۱۰٪ یک هسته (نیازمند hwdec؛ `mesa-va-drivers-freeworld` نصب نیست)
- [ ] RSS کل زیر ۲۰۰ مگابایت — فعلاً ۳۲۹ مگابایت
- [x] resize پنجره بدون سیاه‌شدن / کشیدگی
- [x] خروج تمیز: `mpv_render_context_free` → `mpv_terminate_destroy` بدون segfault
- [x] `ninja -C build check` سبز (smoke واقعی: frames ≥ ۳۰ و position > ۰)

> اعداد در `bench.md`. سه مورد باقی‌مانده همه یک ریشه دارند: hwdec خاموش است.

### دام‌های شناخته‌شده (همین‌ها ۸۰٪ وقت تو را می‌خورند)

| نشانه | علت رایج |
|---|---|
| `mpv_render_param` undeclared | render API در `<mpv/render_gl.h>` است، نه `client.h` |
| `no member named 'framebufferObject'` | در Qt6 عضو protected خودِ `Renderer` است؛ در `synchronize()` با `this->framebufferObject()` |
| `Non-C locale detected` | `setlocale(LC_NUMERIC,"C")` بعد از `QGuiApplication` |
| `start-file` → `end-file` بدون `file-loaded` | فایل قبل از وجود render context لود شده؛ تا `setRenderReady()` عقب بینداز |
| یک فریم رندر شد و `time-pos` روی ۰ قفل شد | `ADVANCED_CONTROL=1` بدون `report_swap()` — خاموش بماند |
| پنجره سیاه، هیچ خطایی نیست | `mpv_render_context_set_update_callback` ست نشده |
| تصویر فقط یک فریم | `update()` صدا زده نمی‌شود / QueuedConnection نیست |
| کرش هنگام resize | id/اندازه‌ی FBO رانه — فقط `synchronize()` آن را معتبر می‌داند |
| تصویر معکوس عمودی | `MPV_RENDER_PARAM_FLIP_Y` را ۰ کن |
| کرش در خروج | آزاد نکردن render context قبل از terminate |
| Vulkan RHI + QQuickFBO | پشتیبانی نمی‌شود → `setGraphicsApi(OpenGL)` |
| `hwdec=no` روی AMD | `mesa-va-drivers-freeworld` از RPM Fusion نصب نیست |
| `qInfo` / `console.log` ساکت | بیلد Qt فدورا `QT_NO_DEBUG_OUTPUT` دارد → از `fprintf(stderr)` استفاده کن |

**سناریوی شکست:** اگر بعد از ۳ روز تصویر نیامد → یک نمونه‌ی عمومی libmpv+QML را build کن و diff بگیر. اگر آن هم نیامد، مشکل محیط است نه کدت.

---

## ۴. فاز ۱ — پلیر پایه (۲ هفته)

هدف: **جایگزین روزمره‌ی VLC برای فایل محلی.**

| # | قابلیت | پیاده‌سازی |
|---|---|---|
| 1 | باز کردن فایل: CLI / drag-drop / dialog | `QCommandLineParser` + `DropArea` |
| 2 | play/pause/seek/volume/speed/frame-step | `Q_INVOKABLE` روی MpvObject |
| 3 | کنترل‌بار + OSD مینیمال | QML سفارشی، auto-hide |
| 4 | انتخاب audio/subtitle track | `track-list` → `QVariantList` |
| 5 | زیرنویس بیرونی + delay + position + font size | `sub-add`, `sub-delay`, `sub-pos` |
| 6 | resume playback | جدول `playback_state` در SQLite |
| 7 | config TOML + hot reload | `toml++` |
| 8 | **MPRIS روی D-Bus** | `QDBusAbstractAdaptor` + `org.mpris.MediaPlayer2.Player` |
| 9 | Screen idle inhibit | `org.freedesktop.ScreenSaver` |
| 10 | مدیا کیبورد | از طریق MPRIS + `QShortcut` |
| 11 | **preset کیبورد PotPlayer در اولین اجرا** | دیالوگ انتخاب keymap |
| 12 | پنجره‌ی همیشه رو + mini-player | `xdg-desktop-portal` |

**DoD:** یک هفته استفاده‌ی روزمره بدون کرش. مدیا کیبورد، resume و زیرنویس بدون دخالت دستی.

---

## ۵. فاز ۲ — ارگونومی PotPlayer (۳ هفته) ← اینجا برنده می‌شوی

| # | قابلیت | نکته‌ی فنی |
|---|---|---|
| 1 | منوی راست‌کلیک کامل | `QMenu` از Go نه — QML `Menu`/`Popup` |
| 2 | seekbar با thumbnail | `ffmpeg -ss N -i f -frames:v 1` در `QThreadPool` + کش دیسک (WebP) |
| 3 | پلی‌لیست + کتابخانه | `QFileSystemWatcher` (inotify) + FTS5 |
| 4 | A-B loop دقیق | `ab-loop-a` / `ab-loop-b` |
| 5 | بوکمارک و فصل | SQLite |
| 6 | زیرنویس دوگانه | دو `sub-add` + `secondary-sub-pos` |
| 7 | جست‌وجوی آنلاین زیرنویس | `QNetworkAccessManager` + OpenSubtitles API |
| 8 | همگام‌سازی خودکار | صدا زدن `ffsubsync` اگر نصب بود؛ وگرنه VAD cross-correlation |
| 9 | اسکرین‌شات/کلیپ → کلیپ‌بورد | `mpv screenshot-raw` + `wl-copy` |
| 10 | پنل اطلاعات فنی | کدک، bitrate، `hwdec-current`، dropped frames، sync |
| 11 | **زیرنویس فارسی تمیز** | `uchardet` → تبدیل به UTF-8 → libass با HarfBuzz |
| 12 | پخش سرعت با حفظ pitch | `audio-pitch-correction=yes` (rubberband) |

**DoD:** کاربر ویندوزی بعد از ۱۰ دقیقه احساس نکند چیزی کم دارد.

---

## ۶. فاز ۳ — تمایز واقعی (۴ هفته)

| # | قابلیت | نکته |
|---|---|---|
| 1 | **HDR passthrough** | `target-colorspace-hint=yes` + Plasma 6 / Wayland color management |
| 2 | tone mapping قابل تنظیم | `tone-mapping=bt.2446a` / spline، `hdr-compute-peak` |
| 3 | shaderها | Anime4K، FSR، deband، `glsl-shaders` |
| 4 | پخش URL + yt-dlp | `ytdl=yes` |
| 5 | EQ و normalization | `lavfi=[equalizer...]` یا `af=lavfi` |
| 6 | watch-party روی LAN | sync engine روی WebSocket |
| 7 | سیستم اسکین | QML theme provider + یک فایل TOML برای رنگ/چیدمان |
| 8 | 360/VR | `video-rotate` + پروفایل‌های libplacebo |

**DoD:** حداقل یک قابلیت که VLC و Haruna ندارند. بدون این، پروژه فقط «یکی دیگر» است.

---

## ۷. فاز ۴ — عرضه (۲ هفته)

- [ ] Flatpak: `org.freedesktop.Platform` + extension `org.freedesktop.Platform.ffmpeg-full` ← **این همان «OpenCodec» توست**
- [ ] AppImage (با `linuxdeploy`)
- [ ] RPM (COPR) + AUR + DEB
- [ ] GitHub Actions: build + `make check` + smoke test روی هر PR
- [ ] README با دو GIF + جدول مقایسه‌ی «PotPlayer vs Parvaz» (بخش ۱۳)
- [ ] crash report اختیاری (breakpad یا فقط backtrace محلی)

**DoD:** `flatpak install` روی یک ماشین تمیز، فایل 4K HEVC بدون هیچ کار اضافه پخش شود.

---

## ۸. قواعد کار با AI (این را در `AGENTS.md` هم کپی کن)

1. **`/usr/include/mpv/client.h` را همیشه در context بگذار.** اسم تابع و enum را از فایل بخواه، نه از حافظه. AI در مورد libmpv با اطمینان بالا اسم جعل می‌کند (`mpv_render_context_render_frame` و امثالش وجود ندارد).
2. **`docs/mpv-properties.md` را از خروجی واقعی بساز:**
   `mpv --list-properties > docs/mpv-properties.md`
3. **هر تغییر باید build و اجرا شود.** «باید کار کند» جواب نیست؛ دستور + خروجی را بخواه.
4. **دو نوع تست:** منطق (config، playlist، زیرنویس، DB) → unit test واقعی با QtTest. مسیر رندر → golden screenshot.
5. **`make check` زیر ۳۰ ثانیه:** build + launch + پخش ۳ ثانیه + exit code.
6. **دیباگ سیستمی، نه حدسی:** `--msg-level=all=v`، `WAYLAND_DEBUG=1`، `MESA_DEBUG=1`، `QSG_INFO=1`، `QT_LOGGING_RULES="qt.quick.*=true"`.
7. **یک commit به ازای هر نقطه‌ی سبز.** agentها تمایل به پخش‌شدگی دارند.
8. **مرز cgo/thread را نشکن.** هر پیشنهادی که mpv را از thread اشتباه صدا بزند، رد کن.
9. **کد QML را کوتاه نگه دار.** منطق در C++، QML فقط چیدمان. QML/JS برای منطق پیچیده = دیباگ جهنمی.

---

## ۹. اندازه‌گیری (اجباری)

`bench.sh` باید هر بار این‌ها را در `bench.md` ثبت کند:

```bash
# RAM پایه (idle با فایل باز)
ps -o rss= -p $(pgrep -f parvaz) | awk '{printf "%.0f MB\n", $1/1024}'

# dropped frames بعد از ۶۰ ثانیه پخش
mpv --term-status-msg='${dropped-frames}/${decoder-frame-drop-count}' ...

# hwdec واقعی
mpv --term-status-msg='${hwdec-current}'
```

**هدف:** idle زیر ۲۰۰MB، ۱۰۸۰p زیر ۲۰۰MB، 4K با hwdec زیر ۳۵۰MB رم + VRAM جدا.
**مقایسه:** VLC ۱۵۰–۲۵۰MB، mpv خالی ۹۰–۱۳۰MB، Electron ۳۰۰MB+.

---

## ۱۰. زمان‌بندی

| فاز | با AI (پاره‌وقت) | با AI (تمام‌وقت) |
|---|---|---|
| ۰. Spike | ۲–۳ روز | ۲ روز |
| ۱. پلیر پایه | ۲ هفته | ۱ هفته |
| ۲. ارگونومی | ۳ هفته | ۲ هفته |
| ۳. تمایز | ۴ هفته | ۲–۳ هفته |
| ۴. عرضه | ۲ هفته | ۱.۵ هفته |
| **جمع** | **~۳ ماه** | **~۲ ماه** |

**حقیقت تلخ:** AI زمان *نوشتن* کد را ۵ برابر می‌کند، زمان *دیباگ گرافیک* را صفر برابر. برنامه‌ات را روی آن ۲۰٪ بچین، نه روی ۸۰٪.

---

## ۱۱. ریسک‌ها

| ریسک | اثر | کاهش |
|---|---|---|
| صفحه‌ی سیاه در Wayland/EGL | تأخیر جدی | فاز ۰ + دام‌های جدول بخش ۳ |
| hwdec روی GPU خاص خراب | بدنامی | `hwdec=auto-safe` + fallback به `no` + گزارش در UI |
| HDR روی کامپوزیتور پشتیبانی‌نشده | crash/رنگ غلط | تشخیص runtime و downgrade خودکار |
| Haruna از قبل وجود دارد | «چرا این؟» | ارگونومی + HDR + زیرنویس فارسی + preset کیبورد |
| AI کد اشتباه با اطمینان بالا | وقت‌کشی | قواعد ۱ و ۲ |
| دامنه‌ی بی‌پایان | هرگز منتشر نمی‌شود | DoD هر فاز؛ فاز بعد فقط بعد از سبز شدن قبلی |

---

## ۱۲. اقدامات بعدی

```bash
# ۱. پیش‌نیازها
sudo dnf install gcc-c++ cmake ninja-build mpv-libs-devel \
  qt6-qtbase-devel qt6-qtdeclarative-devel mesa-libGL-devel \
  libplacebo-devel sqlite-devel

# ۲. تأیید اینکه موتور روی این سیستم سالم است
mpv --hwdec=auto-safe --cache=no sample.mkv
mpv --list-properties > docs/mpv-properties.md

# ۳. ساختار
mkdir -p src/{mpv,ui,library,subs,config,dbus} qml/components qml/Menu \
         qml/Dialogs data docs flatpak

# ۴. CMakeLists.txt + main.cpp + MpvObject + MpvFboRenderer + Main.qml

# ۵. اجرا، ثبت اعداد در bench.md، commit
```

**قانون توقف:** تا فاز ۰ سبز نشده، هیچ فیچری اضافه نکن. وسوسه‌ی «بذار پلی‌لیست را هم بزنم» با AI ده برابر می‌شود، چون نوشتن فیچر جدید همیشه از دیباگ صفحه‌ی سیاه راحت‌تر است.

---

## ۱۳. نام‌گذاری و سئو (تصمیم قطعی)

| مورد | مقدار |
|---|---|
| مخزن | `parvaz-player` |
| نام نمایشی | **Parvaz Player** — پرواز |
| باینری | `parvaz` |
| Flatpak ID | `io.github.<username>.ParvazPlayer` |
| شماره‌گذاری | `v0.1.0` |

### قاعده
> اسم محصول ≠ وصف محصول. «PotPlayer» علامت تجاری Kakao است و نباید در نام مخزن، نام بسته، App ID یا عنوان محصول بیاید (خطر takedown گیت‌هاب + رد شدن در Flathub).
> اما در **وصف** مجاز است: «PotPlayer-style»، «PotPlayer alternative»، «Import PotPlayer keymap».

### سئو، جای درستش

```yaml
description: "PotPlayer-style media player for Linux. mpv-powered, Wayland-native, GPLv3."
topics: potplayer, mpv, video-player, media-player, linux, qt6, qml, wayland, hdr
```

- خط اول README: توضیح یک‌خطی با کلمه‌ی کلیدی.
- جدول «PotPlayer vs Parvaz» در README (گوگل محتوای README را ایندکس می‌کند).
- عنوان صفحه‌ی سایت: «PotPlayer alternative for Linux» نه «PotPlayer for Linux».
- نام فیچر: «Import PotPlayer keymap» — قوی‌ترین قلاب برای کاربر ویندوزی.

### چرا نه نام‌گذاری کلمه‌کلیدی
`potplayer in:name` = ۲۸۹ ریپو · `potplayer linux` = ۱٬۰۵۷ نتیجه · `parvaz player` = ۰.
اسم کلمه‌کلیدی رتبه نمی‌آورد، فقط رقابت اضافه می‌کند.
