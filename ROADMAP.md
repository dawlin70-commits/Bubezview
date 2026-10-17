# 🗺️ BubezView — Feature Roadmap & Architectural Plans

This document tracks planned architectural evolutions, confirmed future milestones, and feature ideas for BubezView.

---

## 1. 🛡️ Integrated Upstream Security & Source Engine Updates
*Proposed & Confirmed: 2026-10-16*

### Objective
Provide automated detection and one-click upgrades for upstream components compiled from source (`libarchive`, `poppler`, `7zip`, `tesseract`, `qpdf`), ensuring users never miss security vulnerability patches (CVEs).

### Implementation Plan
- **Update Dialog (`UpdateDialog` in `src/ui/update_dialog.cpp`):**
  - When checking for updates (or clicking *"Check for updates"*), query the remote versions of compiled source engines alongside the BubezView release.
  - Display an optional section showing component status:
    - *Example:* `LibArchive (compiled v3.7.4 ➔ upstream release v3.7.6 available — contains security fixes)`.
  - Provide a toggle checkbox: `[x] Also update and recompile source engines`.
- **Installer Integration (`install.sh --update`):**
  - If the toggle is active, `install.sh` will fetch the latest upstream git tags, compile and install the updated shared libraries into `~/.local/lib/`, and update BubezView in a single consolidated flow.
  - If unselected, BubezView performs an instant (~3 second) incremental update of only the application binary.

---

## 2. 🎨 Dedicated Advanced Image Editor Plugin (`bubez_image_editor`)
*Proposed & Confirmed: 2026-10-16*

### Objective
Maintain instant cold-boot startup (<15ms) for the core `ImageViewer` while offering a powerful, standalone photo and image manipulation studio without memory overhead.

### Architectural Separation
- **Core Image Viewer (`src/viewers/image/image_viewer.cpp`):**
  - Remains in the Core (100% native Qt6).
  - Handles instant full-screen preview, pan/zoom, basic rotation, and EXIF/metadata display.
  - Zero heavy editing buffers or undo/redo stacks in RAM during casual browsing.
- **Dedicated Plugin (`src/plugins/image_editor/`):**
  - Compiles into `plugins/bubez_image_editor.so` via its own isolated `src/plugins/image_editor/CMakeLists.txt`.
  - Triggered via an *"✏️ Advanced Edit..."* action on the toolbar.
  - Receives the `QImage` or file path and opens the editing canvas (curves, color levels, annotations, privacy blur/pixelation, filters, layers) in a dedicated context or workspace.
  - Clean separation inspired by the DigiKam / Showfoto paradigm.
