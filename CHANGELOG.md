# Changelog

All notable changes to BubezView are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.4.0-beta] - 2026-10-15

### 🚀 Highlights
- **First Public Beta Milestone (`v0.4.0-beta`)**: Transitioned core architecture to an ultra-modular C++ / Qt 6 plugin system with zero fragile dependencies, blazing fast cold boot, and comprehensive desktop integration.
- **Full Internationalization (i18n)**: 100% compliant English codebase with completed Italian localization (2192/2192 strings, 100% finished), plus synchronized catalogs for German, Spanish, and French.

### 🌟 New Features & Enhancements

#### 📄 PDF Engine & Advanced Markup Hub
- **Cryptographic Digital Signatures**:
  - Full support for **PAdES** (ETSI.CAdES.detached SHA-256 embedded into standard `.pdf`).
  - Full support for **CAdES** (`.pdf.p7m` cryptographic envelopes) via OpenSSL.
  - Built-in 1-click generator for self-signed personal X.509 certificates (`.p12` / `.pfx`).
  - Signature verification badge with technical verification output inspector (`pdfsig` / OpenSSL).
- **Interactive Annotation & Markup Suite**:
  - Semi-transparent, multi-color vector highlighters with Ctrl+Z undo support.
  - Vector geometric shapes: Rectangles, Ellipses, Lines, and Directional Arrows with configurable stroke width and fill.
  - Text & Graphic Watermarks: Preset stamps (`CONFIDENTIAL`, `COPY`, `DRAFT`, `APPROVED`, `PAID`, `URGENT`) and custom angles/opacity.
  - Emblem / Logo insertion with gallery favorites and automatic paper background removal (white-to-transparent).
  - Permanent confidential Redaction (Solid Blackout & Clean Whiteout) with draggable resize handles.
- **Page Management**:
  - Insert blank pages (Standard White, 5mm Grid/Quadretti, Ruled/Righe) anywhere in the document.
  - Automatic page overflow detection when shifting content downwards.
  - Sequential PDF merge tool with automatic QPDF font deduplication, linearization, and outline preservation.

#### 📦 Archive Viewer & Compression Suite
- **Interactive Archive Manipulation**:
  - Add files and entire folders directly to existing archives.
  - Remove selected items or directories from archives in-place.
- **7-Zip Multi-Core Hardware Acceleration**:
  - Integrated support for 7-Zip (`7zz`) engine for multi-threaded extraction of large archives at high speeds.
  - Intelligent fallback to `libarchive` when 7-Zip is not present.
- **Modern Compression Wizard**:
  - Create high-speed compressed archives (.zip, .tar.gz, .tar.xz, .tar.zst, .7z) with optional AES-256 encryption.
  - One-click compression and local sharing via LocalSend integration.

#### 🎵 Studio Audio & Media Playback
- **Acoustic Restoration & 4-Tab Audio Suite**:
  - Lossless and multi-format audio trimming (.mp3, .flac, .wav, .aac, .ogg).
  - Automatic silence detection and snap trimming.
  - Automated CUE Sheet splitter into individual named tracks.
  - Pitch Shift (-12 to +12 semitones) and Time Stretch (0.5x to 2.0x playback speed).
  - Vocal & Center Channel remover for karaoke and instrumental extraction.
  - 10-band track equalizer and metadata/album art editor.
- **Bit-Perfect Direct ALSA Hardware Routing**:
  - Direct hardware endpoint selection bypassing software mixers (S/PDIF Optical/Coaxial, Analog Headphone Jack, HDMI endpoints).

#### 🛠️ Architecture, Diagnostics & Maintenance
- **Modular Dynamic Plugins**:
  - Isolated, hot-loadable viewer plugins (`bubez_pdf.so`, `bubez_archive.so`, `bubez_media.so`).
  - Interactive Engine Prompt Card with on-demand plugin activation and source compilation fallback.
- **Multi-Level Diagnostic Tracing**:
  - User-configurable runtime diagnostic levels (Standard User, Lightweight Log, Medium Debug -O1, Deep Debug -O0).
  - One-click complete diagnostics report export to clipboard.
- **Universal Package Manager Integration**:
  - Native helper for Arch (`pacman`), Debian/Ubuntu (`apt`), Fedora (`dnf`), openSUSE (`zypper`), Alpine (`apk`), Void (`xbps`), Gentoo (`emerge`), and Immutable operating systems (SteamOS, Fedora Silverblue / `rpm-ostree`).

### 🧪 Verification & Testing
- Automated test suite (`ctest` and `test_interactive`) passing 100% with zero regressions across 31 full integration scenarios.
- Strict compiler warning verification under GCC 12 (`-O3 -Wall -Wextra -Wpedantic`).

---

## [0.3.0] - 2026-09-28
### Added
- Initial C++ modular rewrite of core viewers.
- High-resolution vector image rendering with zoom and smooth panning.
- Folder gallery and thumbnail strip explorer.
