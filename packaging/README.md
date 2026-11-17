# Packaging — بسته‌های بومی هر توزیع

هر پکیج فقط شامل این‌هاست: باینری `parvaz` + فایل دسکتاپ + AppStream metainfo + آیکون‌های hicolor.
تمام QML، فونت‌ها و شیدرها به‌صورت Qt resource داخل باینری کامپایل می‌شوند و فایل جداگانه‌ای حمل نمی‌کنند.

## وابستگی‌های رانتایم

| نوع | بسته‌ها |
|---|---|
| اجباری | Qt6 (Core, Gui, OpenGL, Quick, QuickControls2, Sql, Network, DBus)، `libmpv`، باینری `ffmpeg`، باینری `unzip` |
| اختیاری | `ffsubsync` (همگام‌سازی زیرنویس)، `yt-dlp` (پخش استریم آنلاین از طریق ytdl hook خود mpv) |

> ملاحظه‌ی نسخه: اپ Qt **≥ 6.5** می‌خواهد. Ubuntu 24.04 با Qt سیستمِ 6.4 نمی‌تواند اجرا شود — برای آن‌جا Flatpak یا نسخه‌های بعدی اوبونتو (25.04+ با Qt 6.8) را هدف بگیر.

## Arch / CachyOS (PKGBUILD)

```bash
# از ریشه‌ی مخزن:
git archive --prefix=parvaz-0.1.0/ HEAD -o packaging/arch/parvaz-0.1.0.tar.gz
cd packaging/arch
makepkg -si          # می‌سازد، نصب می‌کند و وابستگی‌ها را می‌کشد
```

> نکته: روی آرچ، Qt Quick Controls 2 داخل بسته‌ی `qt6-declarative` است — بسته‌ی `qt6-quickcontrols2` جدا وجود ندارد.

## Fedora / RHEL (RPM با CPack)

```bash
sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel qt6-qtdeclarative-devel \
  mpv-libs-devel ffmpeg unzip libgl-devel
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -Dparvaz_packaging=ON
ninja -C build
cpack --config build/CPackConfig.cmake -G RPM
dnf install build/parvaz-0.1.0-*.rpm
```

## Ubuntu 25.04+ (DEB با CPack)

```bash
sudo apt install g++ cmake ninja-build qt6-base-dev qt6-declarative-dev \
  libmpv-dev dpkg-dev ffmpeg unzip libgl-dev
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -Dparvaz_packaging=ON
ninja -C build
cpack --config build/CPackConfig.cmake -G DEB
sudo apt install ./build/parvaz_0.1.0-*.deb
```

> وابستگی‌های کتابخانه‌ای Qt/mpv با `dpkg-shlibdeps` (`CPACK_DEBIAN_PACKAGE_SHLIBDEPS=ON`) از خود باینری کشف می‌شوند؛ `ffmpeg` و `unzip` به‌صورت صریح لیست شده‌اند.

## Flatpak (همه‌ی توزیع‌ها)

```bash
flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak remote-add --if-not-exists kdeapps https://distribute.kde.org/kdeapps.flatpakrepo
flatpak install -y org.kde.Platform//6.8 org.kde.Sdk//6.8 \
  org.freedesktop.Platform.ffmpeg-full//24.08
flatpak-builder --install --force-clean build-flatpak \
  flatpak/io.github.parvaz.ParvazPlayer.yml
flatpak run io.github.parvaz.ParvazPlayer
```

> ران‌تایم `org.kde.Platform` برخلاف `org.freedesktop.Platform` شامل Qt6 است — این همان اصلاحی است که روی مانیفست انجام شد.

## CI

`.github/workflows/packages.yml` فقط روی تگ‌های `v*` و اجرای دستی (`workflow_dispatch`) تمام این‌ها را در کانتینرهای واقعی توزیع‌ها می‌سازد، اسموک را در هر کانتینر اجرا می‌کند و خروجی‌ها را به ریلیز مربوط به همان تگ می‌چسباند.
