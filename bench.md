# bench.md — اندازه‌گیری‌های واقعی Parvaz

> هر عددی که اینجا نیست، ادعا نیست. `./build/parvaz --bench <ثانیه> <فایل>` را دوباره اجرا کن.

## محیط

| مورد | مقدار |
|---|---|
| CPU/GPU | AMD Cezanne (Radeon Vega iGPU)، amdgpu |
| سیستم | Fedora 45، Wayland، GNOME |
| Qt | 6.11.2 |
| mpv | 0.41.0 (libmpv) |
| RAM | 14 GiB (در زمان تست ~۹ GiB اشغال، load average ~5.9) |

## ۲۰۲۶-۱۱-۱۳ — فاز ۰

| تست | frames | position | RSS | dropped | hwdec |
|---|---|---|---|---|---|
| 640x360@30، ۳ ثانیه (smoke) | 73 | 2.37s | — | — | no |
| 1920x1080@60، ۸ ثانیه | 472 / 480 | 8.08s | 329MB | 14 | no |
| 1920x1080@60، ۶ ثانیه (زیر بار سیستم) | 55 | 5.45s | 338MB | 274 | no |

### تفسیر

- ✅ **پخش realtime تأیید شد:** ۴۷۲ فریم از ۴۸۰ در ۸ ثانیه = ۵۹ fps.
- ⚠️ **hwdec فعال نیست:** بسته‌ی `mesa-va-drivers-freeworld` نصب نیست. تا نصب نشود، دیکود نرم‌افزاری است و RSS بالاست.
- ⚠️ **افت فریم زیر بار:** با load average ۵.۹، mpv عامدانه فریم می‌ریزد تا زمان را نگه دارد. این نقص کد نیست، اما با hwdec خیلی بهتر می‌شود.
- ⚠️ **RSS ۳۲۹MB** با دیکود نرم‌افزاری 1080p60. هدف ۲۰۰MB فقط با hwdec واقعی می‌شود.

## هدف فاز ۰

| معیار | هدف | وضعیت |
|---|---|---|
| 1080p60 بدون افت | ۰ dropped | ❌ (۱۴ در ۸ ثانیه، نرم‌افزاری) |
| CPU | < ۱۰٪ یک هسته | ⏳ اندازه‌گیری نشده |
| RSS | < ۲۰۰MB | ❌ ۳۲۹MB |
| resize بدون artifact | — | ✅ دستی تأیید شد |
| خروج تمیز | بدون segfault | ✅ |

## قدم بعدی برای سبز شدن

```bash
sudo dnf install mesa-va-drivers-freeworld libva-utils
vainfo | grep -i h264          # باید H264High پشتیبانی شود
./build/parvaz --bench 8 /tmp/opencode/1080p60.mp4   # باید hwdec=vaapi بدهد
```
