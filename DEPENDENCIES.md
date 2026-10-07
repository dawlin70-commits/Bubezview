# BubezView — System Dependencies Guide

This document tracks all system dependencies required to build, run, and package **BubezView** across major Linux distributions (Arch / Manjaro / EndeavourOS, Debian / Ubuntu, Fedora).

---

## 1. Mandatory Dependencies (Build & Runtime)

These core libraries and tools are required to compile and execute the native C++ Qt6 application:

### Arch Linux / Manjaro / EndeavourOS
```bash
sudo pacman -S --needed \
    base-devel \
    cmake \
    qt6-base \
    qt6-declarative \
    poppler-qt6 \
    libarchive \
    librevenge \
    mpv \
    pkgconf
```

### Debian / Ubuntu
```bash
sudo apt update && sudo apt install -y \
    build-essential \
    cmake \
    qt6-base-dev \
    libqt6opengl6-dev \
    libpoppler-qt6-dev \
    libarchive-dev \
    librevenge-dev \
    libmpv-dev \
    pkg-config
```

### Fedora
```bash
sudo dnf install -y \
    gcc-c++ \
    cmake \
    qt6-qtbase-devel \
    qt6-qtdeclarative-devel \
    poppler-qt6-devel \
    libarchive-devel \
    librevenge-devel \
    mpv-libs-devel \
    pkgconf-pkg-config
```

---

## 2. Recommended Dependencies (Full Feature Set & Desktop Integration)

These optional packages do not prevent the core application from running, but unlock the full fidelity rendering and extended multimedia capabilities:

### 🌟 LibreOffice (`Recommends`)
- **Arch Linux**: `libreoffice-still` (or `libreoffice-fresh`)
- **Debian / Ubuntu**: `libreoffice`
- **Fedora**: `libreoffice`
- **Purpose**: Enables seamless headless conversion for office documents (`.docx`, `.xlsx`, `.xltx`, `.odt`, `.ods`, `.doc`, `.rtf`, `.pptx`). Converted files are rendered by the high-definition PDF engine with exact typography fidelity, enabling toolbar actions:
  - ✏️ *Edit in LibreOffice*
  - 💾 *Save as PDF*
  - 🖼️ *Extract Images*

### 🎬 Multimedia Codecs (FFmpeg & GStreamer) (`Recommends`)
- **Arch Linux**: `ffmpeg`, `gst-plugins-good`, `gst-plugins-bad`, `gst-libav`
- **Debian / Ubuntu**: `ffmpeg`, `gstreamer1.0-plugins-good`, `gstreamer1.0-plugins-bad`, `gstreamer1.0-libav`
- **Fedora**: `ffmpeg`, `gstreamer1-plugins-good`, `gstreamer1-plugins-bad-free`
- **Purpose**: Hardware-accelerated and smooth playback of multimedia video (MP4, MKV, AVI, WebM) and audio (MP3, FLAC, WAV, OGG, Opus) files.

### 🗂️ Desktop File Manager Integration (`Recommends`)
- **Nemo / Cinnamon**: Native Spacebar quick look preview via DBus (`org.nemo.Previewer`).
- **Nautilus / GNOME**: Context menu integration and DBus preview bridge.
- **Dolphin / KDE**: ServiceMenu action and global shortcut trigger (`Ctrl+Space`).

---

## 3. CPack Debian Package Configuration

In `CMakeLists.txt`, `libreoffice` and `ffmpeg` are declared as Debian package recommendations:
```cmake
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "libreoffice, ffmpeg")
```
When an end user installs the `.deb` package, the system package manager will suggest or automatically install LibreOffice if system recommendations are enabled, ensuring a graceful out-of-the-box experience.
