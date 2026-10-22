# Changelog

All notable changes to BubezView are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.4.7] - 2026-10-22

### 🚀 Highlights
- **OTA In-App Update & Installer Fix**: Resolved CMakeLists source directory resolution issue during automatic in-app updates.
- **Robust Standalone Installer Integration**: UpdateManager now dynamically fetches and executes the verified standalone web installer, guaranteeing seamless background updates across all environments.

---

## [0.4.6] - 2026-10-22

### 🚀 Highlights
- **First-Run Wizard Overhaul**: Streamlined onboarding experience with decluttered configuration presets (Complete Suite, Minimal Core, Custom Category Selection), interactive real-time summary card, and responsive layout for compact screens.
- **Native System Media Fallback**: Audio and video files now play seamlessly without requiring MPV or external heavy packages, leveraging OS native system codecs via Qt Multimedia.
- **Photo Studio & Basic Image Retouching Suite**: Added dedicated photo studio with creative presets, scene profiles, adaptive shadow lift, skin tone vitality protection, noise suppression, image resizing, and EXIF/GPS inspector.
- **Complete i18n Localization & Code Quality**: Fully synchronized translation catalogs (100% complete Italian translation with 2,385 strings and 0 unfinished), enhanced automated unit tests, and comprehensive Doxygen documentation.

### 🌟 New Features & Enhancements

#### 🧙 First-Run Wizard Redesign
- **Simplified Modular Presets**:
  - *Complete Suite*: Enables all viewers and full extension packages (LibreOffice, OCR, QPDF, PAdES/CAdES signatures, MPV GPU, 10-Band Equalizer, 7-Zip Multi-Core, extended codecs).
  - *Minimal Core*: Pure lightweight core engines without any heavy external dependencies (zero background bloat).
  - *Custom Selection*: Granular per-category toggles for Documents & Office, Archives & ISO, Video & Movies, Music & Audio, and Images & Graphics.
- **Dedicated Archives & ISO Category**: Added full support for configuring 7-Zip Multi-Core and ISO tools directly in the wizard and settings.
- **Dynamic Live Summary**: Interactive configuration summary card detailing active viewers, enabled extension packages, and future update policy.
- **Responsive Layout**: Re-engineered page layout to cleanly adapt to non-fullscreen and compact window dimensions without visual overlap or awkward scrollbars.

#### 🎨 Photo Studio & Image Processing
- **Creative Filter Presets**: 11 one-click creative filters (Hollywood Cinematic, Classic Antique Sepia, Analog Matte Shadows, Dramatic Monochrome, Deep Sky & Foliage, Soft Flattering Portraits, Warm Amber Sunset, Cyan Glacial, Punchy Lomo, Cyberpunk Night Neon, Clean Document & Text).
- **Intelligent Scene Profiles**: Optimized processing profiles for Natural & Balanced, Vivid Landscapes, Backlight & Shadows, and Dehaze/Atmospheric.
- **Tone & Detail Preservation**:
  - Adaptive Shadows: Dynamic lift of underexposed shadow details for backlit portraits and dark scenes.
  - Skin Tone Protection: Preserves natural peach warmth and prevents human complexion from shifting to cold cyan hues.
  - Shadow Noise Suppression: Removes high-frequency noise from boosted dark areas.
  - Center-Weighted Metering: Prioritizes subject exposure in high-contrast or backlit framing.
  - Eyedropper White Balance Calibration: Instant sample-point neutral color balancing.
- **Image Resizing (`Ctrl+R`)**: Proportional or absolute scaling (pixels or percentage) with live megapixel and delta percentage calculation.
- **EXIF & Properties Inspector (`Alt+Return`)**: Comprehensive photo metadata viewer displaying camera model, lens, exposure, aperture, ISO, GPS coordinates with one-click clipboard copying, and live RGB/luminance histogram.
- **Full Non-Destructive Undo/Redo**: Seamless multi-level history for all transformations, adjustments, and filter operations in `ImageViewer`.

#### 🎬 Native System Media Engine (`SystemMediaViewer`)
- **Lightweight OS Fallback**: Built-in fallback player utilizing `Qt6::Multimedia` and native OS decoders.
- **Zero-Dependency Media Playback**: Audio and video files play immediately even when MPV is not installed or when running in Minimal Core mode.

#### 🧪 Code Cleanup, Quality & Verification
- **Codebase Simplification**: Removed legacy, redundant table widgets and obsolete code paths from the wizard.
- **Full Internationalization (i18n)**: All new UI strings fully localized in English and Italian (2,385 source strings, 100% finished), and synchronized across German, Spanish, and French catalogs.
- **Automated Test Suite Expansion**: Added dedicated tests (`test_exif`, `test_media_system`, and extended `test_image`) covering transformations, EXIF parsing, and system playback with 100% pass rate in <1s.

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
