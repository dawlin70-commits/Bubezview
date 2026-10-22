# BubezView — System Dependencies Guide

This document tracks all system dependencies required to build and run **BubezView** across major Linux distributions (Arch / Manjaro / EndeavourOS, Debian / Ubuntu, Fedora).

---

## 1. Core Mandatory Dependencies (Build & Core Runtime)

These core libraries and tools are the only requirements needed to compile and execute the native C++ Qt6 core application (image, text, and folder viewers, desktop integration, and setup wizard):

### Arch Linux / Manjaro / EndeavourOS
```bash
sudo pacman -S --needed \
    base-devel \
    cmake \
    pkgconf \
    qt6-base \
    qt6-svg \
    qrencode
```

### Debian / Ubuntu
```bash
sudo apt update && sudo apt install -y \
    build-essential \
    cmake \
    pkg-config \
    qt6-base-dev \
    libqt6opengl6-dev \
    libqt6svg6-dev \
    libqrencode-dev
```

### Fedora
```bash
sudo dnf install -y \
    gcc-c++ \
    cmake \
    pkgconf-pkg-config \
    qt6-qtbase-devel \
    qt6-qtsvg-devel \
    qrencode-devel
```

---

## 2. Modular Engines (Optional / Managed via First-Run Wizard)

These engines and plugins are modular: they do not prevent the core application from running and can be selected, compiled, or installed on demand directly from the **First-Run Wizard** or **Settings → Extensions**:

### 📄 PDF Document Engine
- **Debian / Ubuntu**: `libpoppler-qt6-dev`
- **Arch Linux**: `poppler-qt6`
- **Fedora**: `poppler-qt6-devel`
- **Purpose**: Enables vector rendering of PDF pages, annotations, form fill-in, and cryptographic digital signature validation.

### 📦 Archive Manager Engine
- **Debian / Ubuntu**: `libarchive-dev`
- **Arch Linux**: `libarchive`
- **Fedora**: `libarchive-devel`
- **Purpose**: High-speed browsing, tree navigation, and extraction for ZIP, TAR, 7Z, ISO, and compressed archives.

### 🎬 Multimedia Video & Audio Player
- **Debian / Ubuntu**: `libmpv-dev`
- **Arch Linux**: `mpv`
- **Fedora**: `mpv-devel`
- **Purpose**: Hardware GPU-accelerated video playback and high-fidelity bit-perfect audio streaming.

---

## 3. Extended Tools & System Recommendations

These tools extend functionality for specialized workflows:

### 🌟 LibreOffice
- **Debian / Ubuntu**: `libreoffice`
- **Arch Linux**: `libreoffice-still`
- **Fedora**: `libreoffice`
- **Purpose**: Headless conversion of office documents (`.docx`, `.xlsx`, `.pptx`, `.odt`, `.ods`, `.rtf`).

### 🔍 OCR & PDF Utilities
- **Tesseract OCR**: `tesseract-ocr` (extract selectable text from scanned images and snippets).
- **QPDF**: `qpdf` (linearization, decryption, and stream repair).
- **7-Zip**: `7zip` / `p7zip` (multi-threaded compression and modern zstd archive support).

### 🖼️ Extended Image Formats (Qt6 Image Plugins)
- **Debian / Ubuntu**: `qt6-image-formats-plugins`
- **Arch Linux**: `qt6-imageformats`
- **Fedora**: `qt6-imageformats`
- **Source Compilation / Immutable Distros (SteamOS, Silverblue)**: Compilable into `~/.local/lib/qt6/plugins/imageformats/` directly via `./install.sh --build-component image-formats` or the Extensions Settings panel without requiring root or modifying `/usr`.
- **Purpose**: High-efficiency WebP, TIFF, TGA, and Apple ICNS image codec support.
