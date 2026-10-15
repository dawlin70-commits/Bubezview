# BubezView

<p align="center">
  <img src="resources/icons/bubezview.png" alt="BubezView Logo" width="128" height="128" />
</p>

<p align="center">
  <strong>Ultra-fast, modular, and lightweight universal file viewer and productivity hub for Linux desktops.</strong>
</p>

<p align="center">
  <a href="https://github.com/dawlin70-commits/Bubezview/releases"><img src="https://img.shields.io/badge/Release-v0.4.0--beta-blue.svg" alt="Release" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B" alt="C++17" />
  <img src="https://img.shields.io/badge/Qt-6.4+-41CD52?logo=qt" alt="Qt6" />
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-GPLv3-blue.svg" alt="License: GPLv3" /></a>
  <img src="https://img.shields.io/badge/Platform-Linux-FCC624?logo=linux&logoColor=black" alt="Platform Linux" />
  <img src="https://img.shields.io/badge/Architecture-Modular%20Plugins-orange" alt="Architecture" />
</p>

---

## 🌟 Overview

**BubezView** is an extensible, high-performance file viewer engineered from the ground up in modern **C++17** and **Qt 6**. 

Designed to bring the instantaneous, zero-latency **QuickLook preview experience** (pioneered on macOS) to modern Linux desktop environments (such as **KDE Plasma**, **GNOME**, and **Cinnamon**), BubezView enables instant inspection, editing, and markup of documents, media, code, and compressed archives with a single keystroke.

Unlike monolithic preview tools, BubezView features a **modular dynamic plugin architecture** with **graceful soft-dependency degradation**: if an external engine or optional codec is missing, the application never crashes or freezes—instead presenting an integrated prompt allowing one-click package installation or rootless local source compilation.

---

## ✨ Key Features

### 🔍 Desktop Integration & QuickLook Workflow
- **Dolphin File Manager Integration**: Press <kbd>Space</kbd> on any file in KDE Dolphin for an instant, responsive preview without loading heavy standalone applications.
- **Global Desktop Shortcut**: Trigger preview anywhere across your desktop via <kbd>Ctrl</kbd>+<kbd>Space</kbd>.
- **Instant Dismissal**: Close cleanly with a single tap of <kbd>Esc</kbd>.
- **Natural Numeric Sorting**: Implements `QCollator` natural sorting matching file manager behaviors (`ls -v` and Dolphin).

### 📄 PDF Document Studio & Cryptographic Signatures
- **High-Speed Vector Rendering**: Vector-sharp PDF rendering powered by Poppler.
- **Cryptographic Digital Signatures**:
  - Full support for **PAdES** (embedded ETSI.CAdES.detached SHA-256 signatures).
  - Verification and inspection of **CAdES** cryptographic envelopes (`.pdf.p7m`) via OpenSSL.
  - Built-in generator for self-signed personal X.509 digital certificates (`.p12` / `.pfx`).
  - Interactive signature validation badge with technical diagnostic inspection (`pdfsig`).
- **Interactive Annotation & Vector Markup**:
  - Semi-transparent vector highlighters with undo/redo (<kbd>Ctrl</kbd>+<kbd>Z</kbd>).
  - Vector geometric shapes: Rectangles, Ellipses, Lines, and Directional Arrows.
  - Watermark stamps with customizable opacity, rotation, and presets (*CONFIDENTIAL*, *DRAFT*, *APPROVED*, *COPY*).
  - Permanent confidential redaction (Solid Blackout & Clean Whiteout) with draggable resize handles.
  - Insert raster images directly onto PDF pages.
  - Advanced text selection, copying, and area snapshot export.
- **Page Operations**:
  - Insert blank pages anywhere in the document (Plain White, 5mm Grid/Quadretti, Ruled/Righe).
  - Multi-document PDF merge with automatic QPDF font deduplication and linearization.

### 🎬 Studio Multimedia & Audio Suite
- **Zero-Copy Hardware Playback**: Powered by native `libmpv` with GPU hardware acceleration, 4K/60fps support, HDR tone-mapping, and 64-bit float audio pipeline.
- **10-Band Equalizer & Visualizer**: Integrated frequency equalizer and real-time spectrum visualization powered by FFTW3.
- **Bit-Perfect Direct ALSA Routing**: Route audio directly to hardware endpoints (S/PDIF Optical/Coaxial, Analog DAC, HDMI) bypassing software mixers.
- **Acoustic Tools**: Lossless audio trimming, automatic silence detection, and automated CUE sheet track splitting.

### 📦 Compressed Archives Explorer
- **Interactive In-Place Archive Manipulation**: Browse, open, add files/folders to, or delete items from archives without unpacking.
- **Multi-Core 7-Zip Acceleration**: Multi-threaded extraction for large archives via 7-Zip (`7zz`), with seamless fallback to `libarchive`.
- **Modern Compression Hub**: Create compressed archives (`.zip`, `.tar.gz`, `.tar.xz`, `.tar.zst`, `.7z`) with optional AES-256 encryption.
- **Safe Path History**: Never get lost in `/tmp`—navigate back to parent directories or source archive locations with one click.

### 📝 Text & Source Code Editor
- **Syntax Highlighting**: Lightweight, native syntax highlighting for C++, Python, JSON, Markdown, and configuration files with zero external runtime dependencies.
- **Line Numbering**: Synchronized left margin line numbers (`LineNumberArea`).
- **Read-Only / Edit Toggle**: Safe by default; toggle editing mode on demand with a safety prompt on first edit.

### 📁 Folder Browser & Inspector
- **Dolphin-Style Info Panel**: Inspect detailed file system attributes (size, permissions, MIME type, timestamps).
- **High-Resolution Previews**: Native thumbnailing and image rendering for raster and vector graphics.

### 🛠️ Architecture & Hardware Tuning
- **Hot-Loadable Dynamic Plugins**: Isolated modules (`bubez_pdf.so`, `bubez_archive.so`, `bubez_media.so`).
- **CPU Microarchitecture Optimization**: Compile options with `-march=native` instruction tuning and hardware profiles (Balanced, Desktop, Laptop).
- **Diagnostics & Debug Mode**: Comprehensive system health report, soft-dependency inspection, and multi-level runtime logging (`bubezview --debug`).

---

## 💻 System Requirements & Dependencies

### Core Requirements
| Component | Minimum Version | Description |
| :--- | :--- | :--- |
| **Compiler** | C++17 (GCC 9+ / Clang 10+) | Modern C++ standard compliance |
| **Build System** | CMake 3.16+ | Configuration and build pipeline |
| **GUI Framework** | Qt 6.4+ | Core, Gui, Widgets, Network, Svg |

### Optional Engine Dependencies (Soft Dependencies)
BubezView runs seamlessly without these packages, but installing them unlocks specialized modules:

| Engine | Package (Arch) | Package (Debian / Ubuntu) | Feature Unlocked |
| :--- | :--- | :--- | :--- |
| **Poppler Qt6** | `poppler-qt6` | `libpoppler-qt6-dev` | Vector PDF Rendering |
| **LibMPV** | `mpv` | `libmpv-dev` | Video & Audio Playback |
| **LibArchive** | `libarchive` | `libarchive-dev` | Archive Exploration |
| **FFTW3** | `fftw` | `libfftw3-dev` | 10-Band Equalizer & Spectrum |
| **OpenSSL** | `openssl` | `libssl-dev` | Cryptographic Signatures (CAdES/PAdES) |
| **7-Zip** | `7-zip` | `7zip` | Multi-core Archive Acceleration |
| **QPDF** | `qpdf` | `qpdf` | PDF Linearization & Deduplication |
| **Tesseract OCR** | `tesseract` | `tesseract-ocr` | Optical Character Recognition |
| **LibreOffice** | `libreoffice` | `libreoffice` | Office Document Previews (DOCX, ODT) |

---

## 🔨 Building from Source

### 1. Clone the Repository
```bash
git clone https://github.com/dawlin70-commits/Bubezview.git
cd Bubezview
```

### 2. Configure and Compile
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DOPTIMIZATION_LEVEL=O2
cmake --build build -j$(nproc)
```

To optimize instructions specifically for your CPU microarchitecture:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DOPTIMIZATION_LEVEL=O3 -DOPTIMIZE_FOR_NATIVE_CPU=ON
cmake --build build -j$(nproc)
```

### 3. Run Automated Tests
```bash
ctest --test-dir build --output-on-failure
```

---

## 🚀 Installation

### Automated Interactive Installer
BubezView includes an intelligent installation script that detects your Linux distribution, configures necessary system packages, compiles plugins, and registers desktop file associations:

```bash
./install.sh
```

### Manual KDE Dolphin Spacebar Setup (QuickLook)
To configure the <kbd>Space</kbd> key in KDE Dolphin for instant BubezView preview:

1. Create the KIO ServiceMenu directory:
   ```bash
   mkdir -p ~/.local/share/kio/servicemenus
   ```
2. Create `~/.local/share/kio/servicemenus/bubezview.desktop`:
   ```ini
   [Desktop Entry]
   Type=Service
   MimeType=application/octet-stream;
   Actions=preview;
   X-KDE-Shortcuts=Space

   [Desktop Action preview]
   Name=Preview with BubezView
   Icon=bubezview
   Exec=bubezview %u
   ```
3. In KDE Plasma settings, ensure the ServiceMenu shortcut is assigned to <kbd>Space</kbd>. Pressing <kbd>Esc</kbd> inside BubezView closes the preview window instantly.

---

## 🌐 Localization (i18n)

BubezView is fully localized and detects your desktop language automatically:
- 🇬🇧 **English** (Native base catalog)
- 🇮🇹 **Italian** (100% complete)
- 🇩🇪 **German**
- 🇪🇸 **Spanish**
- 🇫🇷 **French**

Language preference can also be manually chosen in **Settings → Appearance & Interface**.

---

## 📄 License & Credits

- BubezView is licensed under the **GNU General Public License v3.0** (GPLv3). See the [LICENSE](LICENSE) file for complete details.
- Developed by **dawlin70** and contributors.
- See `CHANGELOG.md` for full release notes and milestone history.
