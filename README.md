# Parvaz Player

**PotPlayer's ergonomics, on Linux.** A native, Wayland-first video player built on
libmpv and Qt 6 — no Wine, no Electron, no codec packs.

> **Status: pre-alpha.** Phase 0 is green: the window hosts libmpv's OpenGL render
> context and plays 1080p60 at ~59 fps. No packages yet. See [ROADMAP.md](./ROADMAP.md).

## Why this exists

Linux already has excellent playback engines (mpv, GStreamer) and excellent minimal
UIs. What it lacks is PotPlayer's *workflow*: a complete right-click menu,
frame-accurate A-B loops, dual subtitles, a thumbnail seek preview, a real subtitle
pipeline, and keyboard shortcuts that a Windows refugee already has in muscle memory.

Parvaz is that layer, on top of libmpv. It does not reimplement decoding.

## PotPlayer vs Parvaz

| | PotPlayer | Parvaz |
|---|---|---|
| Native Linux | ❌ (Wine) | ✅ Wayland-first |
| Decoder | bundled OpenCodec | libmpv / FFmpeg |
| Hardware decode | DXVA | VA-API, NVDEC, Vulkan Video |
| HDR | ✅ | 🚧 planned |
| Persian / RTL subtitles | partial | ✅ dedicated |
| Scriptable | ❌ | mpv command interface |
| License | proprietary | GPL-3.0 |

## Build

Requirements: Qt 6.5+, libmpv 0.38+, a C++20 compiler, CMake ≥ 3.21.

```bash
# Fedora
sudo dnf install gcc-c++ cmake ninja-build mpv-libs-devel \
  qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtquickcontrols2-devel \
  mesa-libGL-devel libplacebo-devel

# Hardware decoding on AMD/Intel (RPM Fusion)
sudo dnf install mesa-va-drivers-freeworld libva-utils

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
ninja -C build
./build/parvaz /path/to/video.mkv
```

Smoke test — builds, generates a clip, plays it for 3 seconds and fails if nothing
was actually rendered:

```bash
ninja -C build check
```

Benchmark:

```bash
./build/parvaz --bench 8 file.mkv
# parvaz bench: 8s frames=472 pos=8.08s rss=329MB hwdec=no drop=14
```

## Keys

| Key | Action |
|---|---|
| <kbd>Space</kbd> | play / pause |
| <kbd>←</kbd> <kbd>→</kbd> | seek ∓5 s |
| <kbd>↑</kbd> <kbd>↓</kbd> | seek ±30 s |
| <kbd>F</kbd> | toggle fullscreen |
| <kbd>Esc</kbd> | leave fullscreen |
| <kbd>Ctrl</kbd>+<kbd>Q</kbd> | quit |

Drag & drop works. A PotPlayer keymap preset is planned for phase 1.

## Goals

| Metric | Target |
|---|---|
| Idle memory | < 200 MB |
| 1080p60 with hardware decoding | 0 dropped frames |
| Startup to first frame | < 300 ms |

Real measurements, not claims: [bench.md](./bench.md).

## Roadmap

- [x] **Phase 0** — render pipeline, playback, real smoke test, benchmark harness
- [ ] **Phase 1** — control bar, seekbar, playlist, resume, MPRIS
- [ ] **Phase 2** — PotPlayer ergonomics: right-click menu, A-B loop, dual subtitles,
      online subtitle search, auto-sync, Persian subtitle fixes
- [ ] **Phase 3** — HDR, shaders, yt-dlp, watch party
- [ ] **Phase 4** — Flatpak, AppImage, RPM, AUR

Full plan, including the traps that cost a day each: [ROADMAP.md](./ROADMAP.md) and
[AGENTS.md](./AGENTS.md).

## Contributing

Not yet — the architecture is still moving. If you want to help, open an issue
describing your Linux setup and GPU; hardware decode coverage is the main unknown.

## License

GPL-3.0. This is not a preference: libmpv as shipped by distributions is GPL-2.0+,
and linking against it makes the resulting work GPL.

## Trademarks

"PotPlayer" is a trademark of Kakao Corp. Parvaz Player is an independent project,
not affiliated with or endorsed by Kakao. The name is used descriptively only.
