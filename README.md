# Parvaz Player (پرواز)

**PotPlayer's ergonomics, on Linux.** A native, Wayland-first video and music player built on libmpv and Qt 6 — no Wine, no Electron, no codec packs.

[![License: GPL-3.0](https://img.shields.io/badge/License-GPLv3-blue.svg)](./LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![Qt 6](https://img.shields.io/badge/Qt-6.5%2B-green.svg)](https://www.qt.io/)
[![mpv](https://img.shields.io/badge/libmpv-0.38%2B-red.svg)](https://mpv.io/)

> **Status: Alpha — Core Features Implemented.**  
> Phases 0, 1, 2, and key Phase 3 differentiators (HDR tone mapping, AMD FSR upscaling, Debanding, 10-Band EQ with normalizer, 32-band reactive music visualizer, multi-engine online subtitle search, and smart ending credits skip) are fully implemented and green under automated test suites. See [ROADMAP.md](./ROADMAP.md).

---

## Why this exists

Linux already has world-class media playback engines (mpv, FFmpeg) and minimal UIs. What it has lacked is PotPlayer's power-user workflow:
- A comprehensive right-click context menu covering all video, audio, and subtitle controls
- Live seekbar thumbnail previews on hover
- Instant multi-source online subtitle search with Persian mirror support and exact hash matching
- Dedicated Persian / RTL typography using bundled Vazirmatn font and automatic CP1256 detection
- Floating Control Center for real-time video color correction and 10-band audio equalization
- Dedicated Music Player mode with album cover art glow and a dynamic 32-band reactive audio visualizer
- Keyboard shortcuts and seeking multipliers that a Windows refugee already has in muscle memory

Parvaz delivers that experience as a lightweight, native Wayland application on top of libmpv. It does not reimplement decoders — it connects directly to hardware-accelerated APIs with zero frame copying.

---

## PotPlayer vs Parvaz

| Feature | PotPlayer (Windows) | Parvaz Player (Linux) |
|---|---|---|
| Platform | Windows native (Wine on Linux) | Native Linux (Wayland-first / X11) |
| Engine / Decoder | Proprietary OpenCodec / FFmpeg | `libmpv` + `libplacebo` / FFmpeg |
| Frame Pipeline | Direct3D / EVR | Hardware Zero-Copy OpenGL FBO |
| Hardware Decoding | DXVA2 / D3D11VA | VA-API, NVDEC, Vulkan Video |
| Persian / RTL Subtitles | Partial / Misaligned punctuation | First-class: Vazirmatn, CP1256 auto-detect |
| Online Subtitle Search | OpenSubtitles API | OpenSubtitles v3 + Stremio + SubSource |
| Shaders & Upscaling | Custom PixelShaders | AMD FidelityFX Super Resolution (FSR), Deband |
| Audio Equalizer | 10-band EQ | 10-band lavfi EQ + DynAudNorm + 150% Boost |
| Music Mode | Standard UI | Dedicated view + Cover glow + 32-band visualizer |
| Desktop Media Keys | Windows Media Key hooks | Standard Linux MPRIS v2 over D-Bus |
| License | Proprietary Freeware | Free & Open Source (GPL-3.0) |

---

## Features

### 🎬 High-Performance Video Engine
- **Zero-Copy OpenGL Rendering:** Direct presentation into Qt Quick Framebuffer Objects (`QQuickFramebufferObject`).
- **Hardware Acceleration:** Native VA-API, NVDEC, and auto-safe decoding fallbacks.
- **Advanced Shaders & Upscaling:** AMD FidelityFX Super Resolution (FSR) edge-preserving upscaler and 3-stage Debanding.
- **HDR Tone Mapping:** HLG / HDR10 to SDR mapping via Hollywood standards (`bt.2446a`, `spline`, and dynamic peak detection).
- **Pitch-Corrected Speed Scaling:** Smooth playback from 0.1x to 4.0x preserving vocal pitch (`rubberband`).
- **Frame-Accurate Stepping:** Forward and reverse single-frame inspection (<kbd>,</kbd> / <kbd>.</kbd>).

### 💬 Complete Subtitle Pipeline & Persian Typography
- **Vazirmatn Persian Typography:** Plain and SRT subtitles render cleanly using the bundled **Vazirmatn** font.
- **Stylized Subtitle Integrity:** `sub-ass-override="scale"` preserves custom anime styling and ASS effects without distortion.
- **Windows-1256 Codepage Detection:** Auto-decodes legacy Persian/Arabic subtitles encoded in CP1256.
- **Multi-Engine Online Search (<kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>S</kbd>):** Unified search across **OpenSubtitles v3**, **Stremio Catalogs**, and **SubSource** with exact binary hash matching.
- **Automatic Audio Synchronization:** One-click integration with `ffsubsync` to align subtitle timings with speech tracks.
- **Dual Subtitles:** Primary and secondary subtitle positioning and independent delay controls.

### 🎵 Dedicated Music Player View
- **Album Art & Ambient Glow:** Automatic extraction of embedded ID3 / FLAC album covers with dark ambient background glow.
- **Spinning Vinyl Animation:** Elegant spinning vinyl disk fallback when tracks lack embedded cover art.
- **32-Band Reactive Equalizer Visualizer:** Real-time dynamic visualizer mapping 20 Hz – 20 kHz audio frequencies with responsive theme accent gradients.
- **10-Band Equalizer & Normalization:** Built-in presets (Rock, Pop, Jazz, Classical, Flat) and Dynamic Audio Normalizer (`dynaudnorm`).
- **Volume Boost:** Audio amplification up to 150% for quiet recordings.

### ⚡ PotPlayer Ergonomics & Usability
- **Complete Context Menu:** Comprehensive right-click menu with every setting at your fingertips.
- **Hover Seekbar Preview:** Rapid asynchronous thumbnail previews generated on hover via background worker threads.
- **Floating Control Center (<kbd>F7</kbd>):** Real-time adjustment of brightness, contrast, saturation, gamma, audio EQ, and shaders.
- **Slide-in Playlist Drawer (<kbd>F6</kbd>):** Fast search and filter, drag & drop queuing, and recent history.
- **Dual-Zone Drag & Drop:** Top zone (28%) queues to playlist; bottom zone (72%) plays immediately.
- **Smart Ending Credits Skip:** Detects anime/series credit sequences with one-click skip and auto-play prompts.
- **A-B Repeat Looping & Bookmarks:** Frame-accurate loop points and persistent SQLite chapter bookmarks.
- **Desktop Integration:** Screen saver inhibition during playback (`org.freedesktop.ScreenSaver`) and MPRIS v2 D-Bus control.

---

## Keyboard Shortcuts (PotPlayer Standard)

| Shortcut | Action |
|---|---|
| <kbd>Space</kbd> | Play / Pause |
| <kbd>→</kbd> / <kbd>←</kbd> | Seek ±5 seconds |
| <kbd>Ctrl</kbd> + <kbd>→</kbd> / <kbd>←</kbd> | Seek ±30 seconds |
| <kbd>Shift</kbd> + <kbd>→</kbd> / <kbd>←</kbd> | Seek ±60 seconds |
| <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>→</kbd> / <kbd>←</kbd> | Seek ±300 seconds (5 minutes) |
| <kbd>Backspace</kbd> / <kbd>Home</kbd> | Seek to beginning (00:00:00) |
| <kbd>Z</kbd> | Reset speed to normal (1.0x) |
| <kbd>C</kbd> / <kbd>X</kbd> | Adjust speed by ±0.1x |
| <kbd>↑</kbd> / <kbd>↓</kbd> | Volume up / down (5%) |
| <kbd>M</kbd> | Toggle Mute |
| <kbd>F</kbd> / <kbd>Enter</kbd> | Toggle Fullscreen |
| <kbd>1</kbd>, <kbd>2</kbd>, <kbd>3</kbd>, <kbd>4</kbd> | Window size: 0.5x (360p), 1.0x (720p), 1.5x (900p), 2.0x (1080p) |
| <kbd>Tab</kbd> | Toggle Technical Media Info Overlay |
| <kbd>F6</kbd> | Toggle Playlist Drawer |
| <kbd>F7</kbd> | Toggle Floating Control Center |
| <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>S</kbd> | Search Online Subtitles |
| <kbd>Ctrl</kbd> + <kbd>U</kbd> | Open Network URL / Stream |
| <kbd>,</kbd> / <kbd>.</kbd> | Single-frame step backward / forward |
| <kbd>Esc</kbd> | Close dialog / Exit fullscreen |
| <kbd>Ctrl</kbd> + <kbd>Q</kbd> | Quit application |

---

## Build & Install

### Requirements
- **Compiler:** Modern C++20 compiler (`gcc` ≥ 11 or `clang` ≥ 13)
- **Build System:** `cmake` ≥ 3.21, `ninja-build`
- **Qt:** Qt 6.5+ (`qtbase`, `qtdeclarative`, `qtquickcontrols2`)
- **Core Engine:** `mpv-libs` (libmpv 0.38+), `libplacebo`, `mesa-libGL`
- **Database & Media:** `sqlite`, `ffmpeg`

### Fedora / RHEL
```bash
sudo dnf install gcc-c++ cmake ninja-build mpv-libs-devel \
  qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtquickcontrols2-devel \
  mesa-libGL-devel libplacebo-devel sqlite-devel \
  mesa-va-drivers-freeworld libva-utils

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
ninja -C build
./build/parvaz /path/to/media.mkv
```

### Arch Linux / Manjaro
```bash
sudo pacman -S base-devel cmake ninja mpv qt6-base qt6-declarative \
  qt6-quickcontrols2 mesa libplacebo sqlite ffmpeg libva-utils

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
ninja -C build
./build/parvaz /path/to/media.mkv
```

### Automated Smoke Tests
Per Rule 6, Parvaz includes an automated assertion suite verifying video playback, codepage parsing, audio equalization, speed control, bookmarks, and UI wiring:
```bash
ninja -C build check
```

### Benchmarking
Run the built-in benchmark harness:
```bash
./build/parvaz --bench 8 /path/to/sample_1080p60.mp4
# Output: parvaz bench: 8s frames=472 pos=8.08s rss=329MB hwdec=auto drop=0
```

---

## Roadmap

- [x] **Phase 0: Render Engine & Foundation** — Zero-copy OpenGL FBO pipeline, Wayland RHI setup, smoke tests, benchmark harness.
- [x] **Phase 1: Core Player & Shell** — Grounded control bar, seekbar with time formatters, playlist drawer, SQLite resume state, MPRIS v2, idle inhibitor, PotPlayer keymap.
- [x] **Phase 2: PotPlayer Ergonomics** — Right-click context menu, async seekbar thumbnail previews, A-B loop, SQLite bookmarks, dual subtitles, multi-engine online subtitle search, Vazirmatn Persian typography, CP1256 encoding support, auto-sync.
- [x] **Phase 3: Advanced Differentiators** — HDR tone mapping, AMD FSR upscaling, Debanding filters, 10-band audio EQ & DynAudNorm, URL stream playback (Ctrl+U), dedicated Music Player mode with 32-band reactive visualizer, and ending credits skip.
- [ ] **Phase 4: Packaging & Distribution** — Flatpak (`io.github.parvaz.ParvazPlayer`), AppImage, RPM/COPR, and AUR packages.

Full roadmap details: [ROADMAP.md](./ROADMAP.md).

---

## Developer Documentation

Detailed architectural diagrams, threading contracts, subsystem breakdowns, and debugging tips are documented in:
📖 **[docs/DEVELOPER_GUIDE.md](./docs/DEVELOPER_GUIDE.md)**

---

## License

This project is licensed under the **GNU General Public License v3.0 (GPL-3.0)** — see the [LICENSE](./LICENSE) file for details.

### Trademarks
"PotPlayer" is a registered trademark of Kakao Corp. Parvaz Player is an independent open-source project and is not affiliated with, endorsed by, or associated with Kakao Corp. The name is referenced descriptively to indicate keyboard shortcut and workflow compatibility.
