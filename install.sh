#!/usr/bin/env bash
# ==============================================================================
# BubezView — Automated Setup & Build Helper
# Detects distribution dependencies, builds with CMake, and launches BubezView.
# ==============================================================================

set -e

APP_NAME="BubezView"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

# UI Mode Detection (GUI dialogs if available, fallback to CLI)
UI_MODE="cli"
if [ -n "$DISPLAY" ] || [ -n "$WAYLAND_DISPLAY" ]; then
    if command -v zenity >/dev/null 2>&1; then
        UI_MODE="zenity"
    elif command -v kdialog >/dev/null 2>&1; then
        UI_MODE="kdialog"
    elif command -v yad >/dev/null 2>&1; then
        UI_MODE="yad"
    fi
fi

if [ "$1" = "--cli" ] || [ "$BUBEZ_CLI" = "1" ]; then
    UI_MODE="cli"
fi

# Terminal Colors
RED="\033[0;31m"
GREEN="\033[0;32m"
BLUE="\033[0;34m"
YELLOW="\033[1;33m"
BOLD="\033[1m"
NC="\033[0m"

ui_info() {
    local title="$1"
    local msg="$2"
    if [ "$UI_MODE" = "zenity" ]; then
        zenity --info --title="$title" --text="$msg" --width=440 2>/dev/null || true
    elif [ "$UI_MODE" = "kdialog" ]; then
        kdialog --title "$title" --msgbox "$msg" 2>/dev/null || true
    elif [ "$UI_MODE" = "yad" ]; then
        yad --info --title="$title" --text="$msg" --width=440 2>/dev/null || true
    else
        echo -e "${BLUE}${BOLD}[*] ${title}:${NC} ${msg}"
    fi
}

ui_error() {
    local title="$1"
    local msg="$2"
    if [ "$UI_MODE" = "zenity" ]; then
        zenity --error --title="$title" --text="$msg" --width=440 2>/dev/null || true
    elif [ "$UI_MODE" = "kdialog" ]; then
        kdialog --title "$title" --error "$msg" 2>/dev/null || true
    elif [ "$UI_MODE" = "yad" ]; then
        yad --error --title="$title" --text="$msg" --width=440 2>/dev/null || true
    else
        echo -e "${RED}${BOLD}[ERROR] ${title}:${NC} ${msg}"
    fi
}

detect_package_manager() {
    if command -v pacman >/dev/null 2>&1; then echo "pacman"
    elif command -v apt-get >/dev/null 2>&1; then echo "apt"
    elif command -v dnf >/dev/null 2>&1; then echo "dnf"
    elif command -v zypper >/dev/null 2>&1; then echo "zypper"
    else echo "unknown"
    fi
}

install_packages() {
    local pkgs=("$@")
    local PM
    PM=$(detect_package_manager)

    local install_cmd=""
    case "$PM" in
        pacman)
            install_cmd="pacman -S --needed --noconfirm ${pkgs[*]}"
            ;;
        apt)
            install_cmd="apt-get update && apt-get install -y ${pkgs[*]}"
            ;;
        dnf)
            install_cmd="dnf install -y ${pkgs[*]}"
            ;;
        zypper)
            install_cmd="zypper --non-interactive in ${pkgs[*]}"
            ;;
        *)
            ui_error "Package Manager" "Unknown package manager. Please install manually:\n${pkgs[*]}"
            return 1
            ;;
    esac

    echo -e "${BLUE}${BOLD}[*] Installing dependencies with ${PM}...${NC}"
    if [ "$EUID" -eq 0 ]; then
        bash -c "$install_cmd"
    elif command -v pkexec >/dev/null 2>&1; then
        pkexec bash -c "$install_cmd"
    elif command -v sudo >/dev/null 2>&1; then
        sudo bash -c "$install_cmd"
    else
        ui_error "Root Privileges Required" "Cannot execute package installation without root or sudo permissions."
        return 1
    fi
}

check_dependencies() {
    local missing_pkgs=()
    local PM
    PM=$(detect_package_manager)

    case "$PM" in
        pacman)
            command -v cmake >/dev/null 2>&1 || missing_pkgs+=("cmake")
            command -v g++ >/dev/null 2>&1 || missing_pkgs+=("gcc")
            command -v pkg-config >/dev/null 2>&1 || missing_pkgs+=("pkgconf")
            pacman -Q qt6-base >/dev/null 2>&1 || missing_pkgs+=("qt6-base")
            pacman -Q poppler-qt6 >/dev/null 2>&1 || missing_pkgs+=("poppler-qt6")
            pacman -Q libarchive >/dev/null 2>&1 || missing_pkgs+=("libarchive")
            pacman -Q mpv >/dev/null 2>&1 || missing_pkgs+=("mpv")
            ;;
        apt)
            command -v cmake >/dev/null 2>&1 || missing_pkgs+=("cmake")
            command -v g++ >/dev/null 2>&1 || missing_pkgs+=("build-essential")
            command -v pkg-config >/dev/null 2>&1 || missing_pkgs+=("pkg-config")
            dpkg -s qt6-base-dev >/dev/null 2>&1 || missing_pkgs+=("qt6-base-dev" "libqt6opengl6-dev")
            dpkg -s libpoppler-qt6-dev >/dev/null 2>&1 || missing_pkgs+=("libpoppler-qt6-dev")
            dpkg -s libarchive-dev >/dev/null 2>&1 || missing_pkgs+=("libarchive-dev")
            dpkg -s libmpv-dev >/dev/null 2>&1 || missing_pkgs+=("libmpv-dev")
            ;;
        dnf)
            command -v cmake >/dev/null 2>&1 || missing_pkgs+=("cmake")
            command -v g++ >/dev/null 2>&1 || missing_pkgs+=("gcc-c++")
            command -v pkg-config >/dev/null 2>&1 || missing_pkgs+=("pkgconf-pkg-config")
            rpm -q qt6-qtbase-devel >/dev/null 2>&1 || missing_pkgs+=("qt6-qtbase-devel")
            rpm -q poppler-qt6-devel >/dev/null 2>&1 || missing_pkgs+=("poppler-qt6-devel")
            rpm -q libarchive-devel >/dev/null 2>&1 || missing_pkgs+=("libarchive-devel")
            rpm -q mpv-libs-devel >/dev/null 2>&1 || missing_pkgs+=("mpv-libs-devel")
            ;;
        zypper)
            command -v cmake >/dev/null 2>&1 || missing_pkgs+=("cmake")
            command -v g++ >/dev/null 2>&1 || missing_pkgs+=("gcc-c++")
            command -v pkg-config >/dev/null 2>&1 || missing_pkgs+=("pkg-config")
            rpm -q qt6-base-devel >/dev/null 2>&1 || missing_pkgs+=("qt6-base-devel")
            rpm -q poppler-qt6-devel >/dev/null 2>&1 || missing_pkgs+=("poppler-qt6-devel")
            rpm -q libarchive-devel >/dev/null 2>&1 || missing_pkgs+=("libarchive-devel")
            rpm -q mpv-devel >/dev/null 2>&1 || missing_pkgs+=("mpv-devel")
            ;;
    esac

    if [ ${#missing_pkgs[@]} -gt 0 ]; then
        ui_info "Dependency Check" "The following required packages will now be installed:\n\n${missing_pkgs[*]}"
        install_packages "${missing_pkgs[@]}"
    fi
}

build_and_launch() {
    echo -e "${BLUE}${BOLD}[*] Configuring project with CMake...${NC}"
    mkdir -p "$BUILD_DIR"
    cmake -B "$BUILD_DIR" -S "$SCRIPT_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="${HOME}/.local"

    local cpu_cores
    cpu_cores=$(nproc 2>/dev/null || echo "4")
    echo -e "${BLUE}${BOLD}[*] Compiling ${APP_NAME} (${cpu_cores} threads)...${NC}"
    cmake --build "$BUILD_DIR" --parallel "$cpu_cores"

    echo -e "${BLUE}${BOLD}[*] Installing ${APP_NAME} to ${HOME}/.local...${NC}"
    cmake --install "$BUILD_DIR"

    # Ensure desktop databases are updated for instant launcher availability
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "${HOME}/.local/share/applications" 2>/dev/null || true
    fi
    if command -v kbuildsycoca6 >/dev/null 2>&1; then
        kbuildsycoca6 2>/dev/null || true
    fi

    echo -e "${GREEN}${BOLD}[✓] Installation complete! Launching ${APP_NAME}...${NC}"
    
    local executable="${HOME}/.local/bin/bubezview"
    if [ ! -f "$executable" ]; then
        executable="${BUILD_DIR}/bubezview"
    fi

    if [ -f "$executable" ]; then
        nohup "$executable" >/dev/null 2>&1 &
    else
        ui_error "Launch Error" "Could not find built executable at $executable."
        exit 1
    fi
}

main() {
    check_dependencies
    build_and_launch
}

main "$@"
