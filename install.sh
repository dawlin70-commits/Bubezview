#!/usr/bin/env bash
# ==============================================================================
# BubezView — Automated Setup & Build Helper
# Detects distribution dependencies, builds with CMake, and launches BubezView.
# ==============================================================================

set -e

APP_NAME="BubezView"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
if [ -f "/etc/debian_version" ]; then
    BUILD_DIR="${SCRIPT_DIR}/build_deb"
fi

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
            pacman -Q qt6-svg >/dev/null 2>&1 || missing_pkgs+=("qt6-svg")
            pacman -Q qrencode >/dev/null 2>&1 || missing_pkgs+=("qrencode")
            ;;
        apt)
            command -v cmake >/dev/null 2>&1 || missing_pkgs+=("cmake")
            command -v g++ >/dev/null 2>&1 || missing_pkgs+=("build-essential")
            command -v pkg-config >/dev/null 2>&1 || missing_pkgs+=("pkg-config")
            dpkg -s qt6-base-dev >/dev/null 2>&1 || missing_pkgs+=("qt6-base-dev" "libqt6opengl6-dev")
            if ! dpkg -s qt6-svg-dev >/dev/null 2>&1 && ! dpkg -s libqt6svg6-dev >/dev/null 2>&1; then
                if apt-cache show qt6-svg-dev >/dev/null 2>&1; then
                    missing_pkgs+=("qt6-svg-dev")
                else
                    missing_pkgs+=("libqt6svg6-dev")
                fi
            fi
            dpkg -s libqrencode-dev >/dev/null 2>&1 || missing_pkgs+=("libqrencode-dev")
            ;;
        dnf)
            command -v cmake >/dev/null 2>&1 || missing_pkgs+=("cmake")
            command -v g++ >/dev/null 2>&1 || missing_pkgs+=("gcc-c++")
            command -v pkg-config >/dev/null 2>&1 || missing_pkgs+=("pkgconf-pkg-config")
            rpm -q qt6-qtbase-devel >/dev/null 2>&1 || missing_pkgs+=("qt6-qtbase-devel")
            rpm -q qt6-qtsvg-devel >/dev/null 2>&1 || missing_pkgs+=("qt6-qtsvg-devel")
            rpm -q qrencode-devel >/dev/null 2>&1 || missing_pkgs+=("qrencode-devel")
            ;;
        zypper)
            command -v cmake >/dev/null 2>&1 || missing_pkgs+=("cmake")
            command -v g++ >/dev/null 2>&1 || missing_pkgs+=("gcc-c++")
            command -v pkg-config >/dev/null 2>&1 || missing_pkgs+=("pkg-config")
            rpm -q qt6-base-devel >/dev/null 2>&1 || missing_pkgs+=("qt6-base-devel")
            rpm -q qt6-svg-devel >/dev/null 2>&1 || missing_pkgs+=("qt6-svg-devel")
            rpm -q qrencode-devel >/dev/null 2>&1 || missing_pkgs+=("qrencode-devel")
            ;;
    esac

    if [ ${#missing_pkgs[@]} -gt 0 ]; then
        ui_info "Dependency Check" "The following core build packages will now be installed:\n\n${missing_pkgs[*]}"
        install_packages "${missing_pkgs[@]}"
    fi
}

build_source_component() {
    local comp="$1"
    local custom_url="$2"
    local custom_tag="$3"
    local custom_flags="$4"
    local repo_url=""
    local repo_tag="main"
    local cmake_flags=()

    case "$comp" in
        tesseract|ocr|tesseract-ocr)
            repo_url="https://github.com/tesseract-ocr/tesseract.git"
            repo_tag="5.4.1"
            cmake_flags+=("-DBUILD_TRAINEDDATA=OFF" "-DBUILD_TRAINING_TOOLS=OFF" "-DBUILD_TESTS=OFF" "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -march=native -funroll-loops -falign-loops=32 -flto=auto -fomit-frame-pointer -pipe")
            ;;
        qpdf)
            repo_url="https://github.com/qpdf/qpdf.git"
            repo_tag="v11.9.1"
            cmake_flags+=("-DBUILD_STATIC_LIBS=OFF" "-DBUILD_DOC=OFF" "-DREQUIRE_CRYPTO_OPENSSL=ON" "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -march=native -funroll-loops -falign-loops=32 -flto=auto -fomit-frame-pointer -pipe")
            ;;
        libarchive|archive|engine-libarchive)
            repo_url="https://github.com/libarchive/libarchive.git"
            repo_tag="v3.7.4"
            cmake_flags+=("-DENABLE_TEST=OFF" "-DENABLE_TAR=OFF" "-DENABLE_CPIO=OFF" "-DENABLE_CAT=OFF" "-DENABLE_UNZIP=OFF" "-DCMAKE_C_FLAGS_RELEASE=-O3 -march=native -mcrc32 -fno-strict-aliasing -funroll-loops -falign-loops=32 -flto=auto -fomit-frame-pointer -fno-semantic-interposition -pipe")
            ;;
        poppler|engine-poppler)
            repo_url="https://gitlab.freedesktop.org/poppler/poppler.git"
            repo_tag="poppler-26.08.0"
            cmake_flags+=("-DENABLE_QT6=ON" "-DENABLE_QT5=OFF" "-DENABLE_GLIB=OFF" "-DENABLE_CPP=OFF" "-DENABLE_BOOST=OFF" "-DENABLE_UTILS=ON" "-DENABLE_NSS3=OFF" "-DENABLE_GPGME=OFF" "-DENABLE_LIBCURL=OFF" "-DBUILD_QT6_TESTS=OFF" "-DBUILD_CPP_TESTS=OFF" "-DBUILD_MANUAL_TESTS=OFF" "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -march=native -mcrc32 -funroll-loops -falign-loops=32 -flto=auto -fomit-frame-pointer -fno-semantic-interposition -pipe")
            ;;
        7zip|7z|7zz)
            repo_url="https://github.com/ip7z/7zip.git"
            repo_tag="24.08"
            ;;
        image-formats|qtimageformats|qt6-imageformats|qt6-image-formats-plugins)
            comp="qtimageformats"
            repo_url="https://github.com/qt/qtimageformats.git"
            local qt_sys_ver
            qt_sys_ver=$(pkg-config --modversion Qt6Core 2>/dev/null || qmake6 -query QT_VERSION 2>/dev/null || echo "")
            if [ -n "$qt_sys_ver" ]; then
                repo_tag="v${qt_sys_ver}"
            else
                repo_tag="v6.8.0"
            fi
            cmake_flags+=("-DCMAKE_CXX_FLAGS_RELEASE=-O3 -march=native -funroll-loops -falign-loops=32 -flto=auto -fomit-frame-pointer -pipe")
            ;;
        *)
            echo -e "${RED}${BOLD}[!] Error: Component '${comp}' does not have a source build recipe in install.sh.${NC}"
            echo -e "${RED}    Supported compilable source components are: poppler, engine-poppler, libarchive, engine-libarchive, 7zip, qpdf, tesseract, image-formats.${NC}"
            exit 1
            ;;
    esac

    # Apply custom overrides if specified
    if [ -n "$custom_url" ]; then
        repo_url="$custom_url"
    fi
    if [ -n "$custom_tag" ]; then
        repo_tag="$custom_tag"
    fi
    if [ -n "$custom_flags" ]; then
        eval "extra_flags_array=($custom_flags)"
        cmake_flags+=("${extra_flags_array[@]}")
    fi

    echo -e "${BLUE}${BOLD}[*] Compiling ${comp} (${repo_tag}) from source for user environment (${HOME}/.local)...${NC}"
    echo -e "${BLUE}    Source: ${repo_url}${NC}"
    if [ -n "$custom_flags" ]; then
        echo -e "${BLUE}    Extra Flags: ${custom_flags}${NC}"
    fi

    local COMP_SRC="${SCRIPT_DIR}/build_deps/${comp}"
    local COMP_BUILD="${SCRIPT_DIR}/build_deps/${comp}_build"
    mkdir -p "${SCRIPT_DIR}/build_deps"
    if [ ! -d "$COMP_SRC/.git" ]; then
        if ! git clone --depth 1 -b "$repo_tag" "$repo_url" "$COMP_SRC" 2>/dev/null; then
            echo -e "${YELLOW}[!] Tag '${repo_tag}' not found, falling back to 6.8 branch or default HEAD...${NC}"
            git clone --depth 1 -b "6.8" "$repo_url" "$COMP_SRC" 2>/dev/null || git clone --depth 1 "$repo_url" "$COMP_SRC"
        fi
    else
        # Ensure we checkout the correct repo tag if it differs from current HEAD
        (cd "$COMP_SRC" && git fetch --depth 1 origin "$repo_tag" 2>/dev/null || true && git checkout "$repo_tag" 2>/dev/null || true)
    fi
    local cpu_cores_sub
    cpu_cores_sub=$(nproc 2>/dev/null || echo "4")

    if [ "$comp" = "7zip" ] || [ "$comp" = "7z" ] || [ "$comp" = "7zz" ]; then
        local flags_7z="-O3 -march=native -mcrc32 -funroll-loops -falign-loops=32 -flto -fomit-frame-pointer -pipe"
        if [ -n "$custom_flags" ]; then
            flags_7z="$custom_flags"
        fi
        echo -e "${BLUE}${BOLD}[*] Compiling 7-Zip (7zz) via makefile.gcc (${cpu_cores_sub} threads)...${NC}"
        (cd "${COMP_SRC}/CPP/7zip/Bundles/Alone2" && make -j"${cpu_cores_sub}" -f makefile.gcc LOCAL_FLAGS="${flags_7z}")
        mkdir -p "${HOME}/.local/bin"
        install -m 755 "${COMP_SRC}/CPP/7zip/Bundles/Alone2/_o/7zz" "${HOME}/.local/bin/7zz"
        echo -e "${GREEN}${BOLD}[✓] Component '7zz' successfully installed into ${HOME}/.local/bin/7zz!${NC}"
        exit 0
    fi

    mkdir -p "$COMP_BUILD"
    if ! cmake -B "$COMP_BUILD" -S "$COMP_SRC" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="${HOME}/.local" -DCMAKE_INSTALL_RPATH='$ORIGIN/../lib:$ORIGIN/../lib64' -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=TRUE -DCMAKE_BUILD_WITH_INSTALL_RPATH=TRUE "${cmake_flags[@]}"; then
        echo -e "${YELLOW}[!] CMake configure encountered errors; cleaning stale build cache and retrying...${NC}"
        rm -rf "${COMP_BUILD}"
        mkdir -p "${COMP_BUILD}"
        cmake -B "$COMP_BUILD" -S "$COMP_SRC" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="${HOME}/.local" -DCMAKE_INSTALL_RPATH='$ORIGIN/../lib:$ORIGIN/../lib64' -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=TRUE -DCMAKE_BUILD_WITH_INSTALL_RPATH=TRUE "${cmake_flags[@]}"
    fi
    cmake --build "$COMP_BUILD" --parallel "$cpu_cores_sub"
    cmake --install "$COMP_BUILD"
    echo -e "${GREEN}${BOLD}[✓] Component '${comp}' successfully installed into ${HOME}/.local!${NC}"
    exit 0
}

build_and_launch() {
    local target_comp=""
    local custom_git_url=""
    local custom_git_branch=""
    local custom_build_flags=""
    local cmake_extra_args=()
    local build_target=""
    while [ $# -gt 0 ]; do
        case "$1" in
            --target)
                shift
                build_target="$1"
                ;;
            --build-component)
                shift
                target_comp="$1"
                ;;
            --git-url)
                shift
                custom_git_url="$1"
                ;;
            --git-branch)
                shift
                custom_git_branch="$1"
                ;;
            --extra-build-flags)
                shift
                custom_build_flags="$1"
                ;;
            --no-launch)
                no_launch=1
                ;;
            --update)
                no_launch=1
                ;;
            --build-tesseract)
                target_comp="tesseract"
                ;;
            -D*)
                cmake_extra_args+=("$1")
                ;;
        esac
        shift
    done

    if [ -n "$target_comp" ]; then
        build_source_component "$target_comp" "$custom_git_url" "$custom_git_branch" "$custom_build_flags"
    fi

    echo -e "${BLUE}${BOLD}[*] Configuring project with CMake...${NC}"
    mkdir -p "$BUILD_DIR"
    cmake -B "$BUILD_DIR" -S "$SCRIPT_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="${HOME}/.local" "${cmake_extra_args[@]}"

    local cpu_cores
    cpu_cores=$(nproc 2>/dev/null || echo "4")
    if [ -n "$build_target" ]; then
        echo -e "${BLUE}${BOLD}[*] Compiling target '${build_target}' (${cpu_cores} threads)...${NC}"
        cmake --build "$BUILD_DIR" --target "$build_target" --parallel "$cpu_cores"
    else
        echo -e "${BLUE}${BOLD}[*] Compiling ${APP_NAME} (${cpu_cores} threads)...${NC}"
        cmake --build "$BUILD_DIR" --parallel "$cpu_cores"
    fi

    echo -e "${BLUE}${BOLD}[*] Installing ${APP_NAME} to ${HOME}/.local...${NC}"
    cmake --install "$BUILD_DIR"

    # Salva una copia di install.sh per aggiornamenti automatici
    mkdir -p "${HOME}/.local/share/bubezview"
    if [ -f "${SCRIPT_DIR}/install.sh" ]; then
        cp -f "${SCRIPT_DIR}/install.sh" "${HOME}/.local/share/bubezview/install.sh" 2>/dev/null || true
        chmod +x "${HOME}/.local/share/bubezview/install.sh" 2>/dev/null || true
    fi

    # Ensure desktop databases are updated for instant launcher availability
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "${HOME}/.local/share/applications" 2>/dev/null || true
    fi
    if command -v kbuildsycoca6 >/dev/null 2>&1; then
        kbuildsycoca6 2>/dev/null || true
    fi

    if [ "$no_launch" = "1" ]; then
        echo -e "${GREEN}${BOLD}[✓] Compilation and installation finished successfully.${NC}"
        exit 0
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
    build_and_launch "$@"
}

main "$@"
