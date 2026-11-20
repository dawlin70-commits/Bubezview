#!/bin/env bash
# ==============================================================================
# BUBEZVIEW STANDALONE INSTALLER & COMPILER
# ==============================================================================

set -e

APP_ID="bubezview"
APP_NAME="BubezView"
PROJECT_URL="https://dawlinshare.web.app/versioni.json"
TARGET_EXE="bubezview"
APP_CATEGORY="Utility"

RELEASE_PUBKEY="-----BEGIN PUBLIC KEY-----
MCowBQYDK2VwAyEAfr3yn0QMgvYvEjW+fkfKAF8ZP4EWsn9DOrhtjHPEe1I=
-----END PUBLIC KEY-----"

# ------------------------------------------------------------------------------
# 1. PARSING ARGOMENTI & RILEVAMENTO INTERFACCIA
# ------------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

UI_MODE="cli"
FORCE_CLI=0
IS_UPDATE=0
NO_SUDO=0
ACTION=""
PURGE=0
TARGET_COMPONENT=""
BUILD_ONLY_PLUGIN=""
CUSTOM_GIT_URL=""
CUSTOM_GIT_BRANCH=""
CUSTOM_BUILD_FLAGS=""
EXTRA_CMAKE_ARGS=()

while [ "$#" -gt 0 ]; do
    case "$1" in
        --cli|-c)
            FORCE_CLI=1
            ;;
        --update|-u|--yes|-y)
            IS_UPDATE=1
            FORCE_CLI=1
            ;;
        --no-sudo)
            NO_SUDO=1
            ;;
        --uninstall)
            ACTION="uninstall"
            FORCE_CLI=1
            ;;
        --purge)
            PURGE=1
            ;;
        --clean)
            ACTION="clean"
            FORCE_CLI=1
            ;;
        --reinstall)
            ACTION="reinstall"
            FORCE_CLI=1
            ;;
        --build-component)
            if [ "$#" -lt 2 ] || [ -z "$2" ]; then
                echo -e "\033[0;31m[!] Errore: argomento mancante per $1\033[0m" >&2
                exit 1
            fi
            ACTION="build_component"
            FORCE_CLI=1
            TARGET_COMPONENT="$2"
            shift
            ;;
        --git-url)
            if [ "$#" -lt 2 ] || [ -z "$2" ]; then
                echo -e "\033[0;31m[!] Errore: argomento mancante per $1\033[0m" >&2
                exit 1
            fi
            CUSTOM_GIT_URL="$2"
            shift
            ;;
        --git-branch)
            if [ "$#" -lt 2 ] || [ -z "$2" ]; then
                echo -e "\033[0;31m[!] Errore: argomento mancante per $1\033[0m" >&2
                exit 1
            fi
            CUSTOM_GIT_BRANCH="$2"
            shift
            ;;
        --extra-build-flags)
            if [ "$#" -lt 2 ] || [ -z "$2" ]; then
                echo -e "\033[0;31m[!] Errore: argomento mancante per $1\033[0m" >&2
                exit 1
            fi
            CUSTOM_BUILD_FLAGS="$2"
            shift
            ;;
        --build-tesseract)
            ACTION="build_component"
            TARGET_COMPONENT="tesseract"
            FORCE_CLI=1
            ;;
        --build-plugin)
            BUILD_ONLY_PLUGIN="${2:-}"
            FORCE_CLI=1
            IS_UPDATE=1
            shift
            ;;
        -D*)
            EXTRA_CMAKE_ARGS+=("$1")
            ;;
        --help|-h)
            echo "Uso: ./Install.sh [opzioni]"
            echo ""
            echo "Opzioni principali:"
            echo "  (nessun argomento)     Avvia il menu grafico interattivo (Installa, Reinstalla, Disinstalla)"
            echo "  --cli, -c              Esegui in modalità terminale"
            echo "  --update, -u, -y       Aggiorna/installa senza conferme"
            echo "  --reinstall            Reinstalla da zero e riavvia il Setup (First Run Wizard)"
            echo "  --uninstall [--purge]  Disinstalla BubezView dal sistema (con --purge rimuove config e cache)"
            echo "  --clean                Pulisci i file temporanei di compilazione"
            echo "  --no-sudo              Modalità utente pura senza mai chiedere permessi di root/sudo"
            echo "  --build-component <N>  Installa/compila un modulo in Home (poppler, poppler-data, libarchive, 7zip, qpdf, tesseract, tessdata, zstd, xz, mpv)"
            echo "  -h, --help             Mostra questo messaggio di aiuto"
            exit 0
            ;;
    esac
    shift
done

if [ "$FORCE_CLI" -eq 0 ] && ([ -n "$DISPLAY" ] || [ -n "$WAYLAND_DISPLAY" ]); then
    if command -v zenity >/dev/null 2>&1; then
        UI_MODE="zenity"
    elif command -v kdialog >/dev/null 2>&1; then
        UI_MODE="kdialog"
    elif command -v yad >/dev/null 2>&1; then
        UI_MODE="yad"
    fi
fi

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
        zenity --info --title="$title" --text="$msg" --width=400 2>/dev/null || true
    elif [ "$UI_MODE" = "kdialog" ]; then
        kdialog --title "$title" --msgbox "$msg" 2>/dev/null || true
    elif [ "$UI_MODE" = "yad" ]; then
        yad --info --title="$title" --text="$msg" --width=400 2>/dev/null || true
    else
        echo -e "${BLUE}${BOLD}[*] ${title}:${NC} ${msg}"
    fi
}

ui_error() {
    local title="$1"
    local msg="$2"
    if [ "$UI_MODE" = "zenity" ]; then
        zenity --error --title="$title" --text="$msg" --width=420 2>/dev/null || true
    elif [ "$UI_MODE" = "kdialog" ]; then
        kdialog --title "$title" --error "$msg" 2>/dev/null || true
    elif [ "$UI_MODE" = "yad" ]; then
        yad --error --title="$title" --text="$msg" --width=420 2>/dev/null || true
    else
        echo -e "${RED}${BOLD}[ERRORE] ${title}:${NC} ${msg}"
    fi
}

ui_ask_yesno() {
    local title="$1"
    local msg="$2"
    if [ "$IS_UPDATE" -eq 1 ]; then
        return 0
    fi
    if [ "$UI_MODE" = "zenity" ]; then
        zenity --question --title="$title" --text="$msg" --width=400 2>/dev/null
        return $?
    elif [ "$UI_MODE" = "kdialog" ]; then
        kdialog --title "$title" --yesno "$msg" 2>/dev/null
        return $?
    elif [ "$UI_MODE" = "yad" ]; then
        yad --question --title="$title" --text="$msg" --width=400 2>/dev/null
        return $?
    else
        echo -en "${YELLOW}${BOLD}[?] ${title}:${NC} ${msg} [S/n]: "
        read -r resp
        case "$resp" in
            [nN][oO]|[nN]) return 1 ;;
            *) return 0 ;;
        esac
    fi
}

# ------------------------------------------------------------------------------
# 2. CONTROLLO STRUMENTI E AMBIENTE DI COMPILAZIONE
# ------------------------------------------------------------------------------
detect_distro_family() {
    if [ -f /run/ostree-booted ] || [ -f /etc/NIXOS ] || [ -d /nix/store ] || grep -qiE 'silverblue|kinoite|microos|steamos|nixos' /etc/os-release 2>/dev/null; then
        echo "immutable"
        return
    fi
    if [ -f /etc/gentoo-release ] || grep -qi "gentoo" /etc/os-release 2>/dev/null; then
        echo "gentoo"
        return
    fi
    if command -v apt-get >/dev/null 2>&1; then echo "apt"
    elif command -v dnf >/dev/null 2>&1; then echo "dnf"
    elif command -v pacman >/dev/null 2>&1; then echo "pacman"
    elif command -v zypper >/dev/null 2>&1; then echo "zypper"
    elif command -v xbps-install >/dev/null 2>&1; then echo "void"
    elif command -v eopkg >/dev/null 2>&1; then echo "solus"
    elif command -v apk >/dev/null 2>&1; then echo "alpine"
    else echo "unknown"
    fi
}

install_missing_packages() {
    local pkgs=("$@")
    if [ "${NO_SUDO:-0}" -eq 1 ]; then
        echo -e "${RED}${BOLD}[!] Modalità Zero-Sudo attiva:${NC} installazione di sistema bloccata per: ${pkgs[*]}" >&2
        return 1
    fi
    local PM
    PM=$(detect_distro_family)
    
    if [ "$PM" = "immutable" ]; then
        ui_info "Sistema Immutabile Rilevato" "Il file system di sistema è in sola lettura (OSTree / NixOS / SteamOS).\n\nProcedo compilando con i motori incorporati nella cartella utente."
        return 0
    fi
    if [ "$PM" = "gentoo" ]; then
        ui_info "Gentoo Linux Rilevato" "Installa i pacchetti tramite portage:\nsudo emerge --ask ${pkgs[*]}"
        return 0
    fi

    local install_cmd=""
    case "$PM" in
        apt)
            install_cmd="apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y ${pkgs[*]}"
            ;;
        dnf)
            install_cmd="dnf install -y ${pkgs[*]}"
            ;;
        pacman)
            install_cmd="pacman -Sy --noconfirm --needed ${pkgs[*]}"
            ;;
        zypper)
            install_cmd="zypper --non-interactive in ${pkgs[*]}"
            ;;
        void)
            install_cmd="xbps-install -y ${pkgs[*]}"
            ;;
        solus)
            install_cmd="eopkg install -y ${pkgs[*]}"
            ;;
        alpine)
            install_cmd="apk add ${pkgs[*]}"
            ;;
        *)
            ui_error "Installatore Pacchetti" "Gestore pacchetti non supportato. Installa manualmente: ${pkgs[*]}"
            return 1
            ;;
    esac

    if [ "$EUID" -eq 0 ]; then
        bash -c "$install_cmd"
    elif command -v pkexec >/dev/null 2>&1; then
        pkexec bash -c "$install_cmd"
    elif command -v sudo >/dev/null 2>&1; then
        sudo bash -c "$install_cmd"
    else
        ui_error "Permessi insufficienti" "Sono necessari i privilegi di root per installare: ${pkgs[*]}"
        return 1
    fi
}

check_build_tools() {
    local missing_tools=()
    local missing_libs=()
    local PM
    PM=$(detect_distro_family)

    command -v curl >/dev/null 2>&1 || missing_tools+=("curl")
    command -v tar >/dev/null 2>&1 || missing_tools+=("tar")
    command -v cmake >/dev/null 2>&1 || missing_tools+=("cmake")
    command -v g++ >/dev/null 2>&1 || command -v clang++ >/dev/null 2>&1 || {
        case "$PM" in
            apt) missing_tools+=("build-essential" "g++") ;;
            pacman) missing_tools+=("base-devel" "gcc") ;;
            dnf) missing_tools+=("gcc-c++" "make") ;;
            *) missing_tools+=("g++") ;;
        esac
    }
    command -v pkg-config >/dev/null 2>&1 || {
        case "$PM" in
            apt) missing_tools+=("pkg-config") ;;
            pacman) missing_tools+=("pkgconf") ;;
            *) missing_tools+=("pkg-config") ;;
        esac
    }

    # Verifica presenza librerie di sviluppo C++ / Qt6 via pkg-config
    check_pkg_dev() {
        local pkg_name="$1"
        pkg-config --exists "$pkg_name" 2>/dev/null
    }

    if command -v pkg-config >/dev/null 2>&1; then
        case "$PM" in
            apt)
                check_pkg_dev "Qt6Core" || missing_libs+=("qt6-base-dev" "qt6-base-dev-tools")
                check_pkg_dev "Qt6Svg" || missing_libs+=("qt6-svg-dev")
                check_pkg_dev "Qt6Multimedia" || missing_libs+=("qt6-multimedia-dev")
                check_pkg_dev "poppler-qt6" || missing_libs+=("libpoppler-qt6-dev")
                check_pkg_dev "libqrencode" || missing_libs+=("libqrencode-dev")
                ;;
            pacman)
                check_pkg_dev "Qt6Core" || missing_libs+=("qt6-base")
                check_pkg_dev "Qt6Svg" || missing_libs+=("qt6-svg")
                check_pkg_dev "Qt6Multimedia" || missing_libs+=("qt6-multimedia")
                check_pkg_dev "poppler-qt6" || missing_libs+=("poppler-qt6")
                check_pkg_dev "libqrencode" || missing_libs+=("qrencode")
                ;;
            dnf)
                check_pkg_dev "Qt6Core" || missing_libs+=("qt6-qtbase-devel")
                check_pkg_dev "Qt6Svg" || missing_libs+=("qt6-qtsvg-devel")
                check_pkg_dev "Qt6Multimedia" || missing_libs+=("qt6-qtmultimedia-devel")
                check_pkg_dev "poppler-qt6" || missing_libs+=("poppler-qt6-devel")
                check_pkg_dev "libqrencode" || missing_libs+=("qrencode-devel")
                ;;
        esac
    else
        if [ "$PM" = "apt" ]; then
            missing_libs+=("qt6-base-dev" "qt6-base-dev-tools" "qt6-svg-dev" "qt6-multimedia-dev" "libpoppler-qt6-dev" "libqrencode-dev")
        elif [ "$PM" = "pacman" ]; then
            missing_libs+=("qt6-base" "qt6-svg" "qt6-multimedia" "poppler-qt6" "qrencode")
        elif [ "$PM" = "dnf" ]; then
            missing_libs+=("qt6-qtbase-devel" "qt6-qtsvg-devel" "qt6-qtmultimedia-devel" "poppler-qt6-devel" "qrencode-devel")
        fi
    fi

    local all_missing=("${missing_tools[@]}" "${missing_libs[@]}")
    if [ ${#all_missing[@]} -gt 0 ]; then
        ui_info "Installazione Dipendenze" "Per compilare l'applicazione è necessario installare i seguenti pacchetti di sistema:
${all_missing[*]}

Procedo con l'installazione automatica..."
        install_missing_packages "${all_missing[@]}" || {
            ui_error "Errore" "Impossibile installare i pacchetti richiesti. Installa manualmente: ${all_missing[*]}"
            exit 1
        }
    fi
}

# ------------------------------------------------------------------------------
# 3. DOWNLOAD SORGENTI DA RELEASE O CLOUD
# ------------------------------------------------------------------------------
download_source_payload() {
    local source_url="$1"
    local dest_file="$2"

    if [[ "$source_url" =~ drive\.google\.com ]]; then
        local file_id=""
        if [[ "$source_url" =~ /d/([^/]+) ]]; then
            file_id="${BASH_REMATCH[1]}"
        elif [[ "$source_url" =~ id=([^&]+) ]]; then
            file_id="${BASH_REMATCH[1]}"
        fi

        if [ -z "$file_id" ]; then
            ui_error "Download" "Link di Google Drive non valido o non trovato."
            exit 1
        fi

        local cookie_file
        cookie_file=$(mktemp)
        
        local confirm_token
        confirm_token=$(curl -s -c "$cookie_file" -L "https://drive.google.com/uc?export=download&id=${file_id}" | \
            grep -o 'name="confirm" value="[^"]*"' | head -n 1 | sed -e 's/name="confirm" value="//' -e 's/"//' || true)

        if [ -z "$confirm_token" ]; then
            confirm_token=$(curl -s -c "$cookie_file" -L "https://drive.google.com/uc?export=download&id=${file_id}" | \
                grep -o 'confirm=[0-9a-zA-Z_]\+' | head -n 1 | cut -d'=' -f2 || true)
        fi

        local download_url="https://drive.google.com/uc?export=download&id=${file_id}"
        if [ -n "$confirm_token" ]; then
            download_url="https://drive.google.com/uc?export=download&confirm=${confirm_token}&id=${file_id}"
        fi

        if [ "$UI_MODE" = "zenity" ]; then
            (
                curl -b "$cookie_file" -L "$download_url" -o "$dest_file" 2>&1 | \
                stdbuf -oL tr '\r' '\n' | \
                grep --line-buffered -o '[0-9]*\.[0-9]*%' | \
                sed -u 's/\..*//'
            ) | zenity --progress --title="Scaricamento $APP_NAME" --text="Download dei sorgenti in corso..." --percentage=0 --auto-close 2>/dev/null || true
        else
            echo -e "${BLUE}[*] Scaricamento dei sorgenti...${NC}"
            curl -b "$cookie_file" -L --progress-bar "$download_url" -o "$dest_file"
        fi

        rm -f "$cookie_file"
    else
        # Download diretto ad alta velocità (GitHub Releases o CDN diretta)
        if [ "$UI_MODE" = "zenity" ]; then
            (
                curl -L "$source_url" -o "$dest_file" 2>&1 | \
                stdbuf -oL tr '\r' '\n' | \
                grep --line-buffered -o '[0-9]*\.[0-9]*%' | \
                sed -u 's/\..*//'
            ) | zenity --progress --title="Scaricamento $APP_NAME" --text="Download dei sorgenti in corso..." --percentage=0 --auto-close 2>/dev/null || true
        else
            echo -e "${BLUE}[*] Scaricamento dei sorgenti...${NC}"
            curl -L --progress-bar "$source_url" -o "$dest_file"
        fi
    fi

    if [ ! -s "$dest_file" ]; then
        ui_error "Download fallito" "Il download non ha prodotto un file valido."
        exit 1
    fi
}

# ------------------------------------------------------------------------------
# 4. COMPILAZIONE COMPONENTI ON-DEMAND DA SORGENTE (Richiamata da BubezView C++)
# ------------------------------------------------------------------------------
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
            cmake_flags+=("-DBUILD_TRAINEDDATA=OFF" "-DBUILD_TRAINING_TOOLS=OFF" "-DBUILD_TESTS=OFF" "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -march=native -funroll-loops -falign-loops=32 -flto=auto -fomit-frame-pointer -pipe -include cstdint")
            ;;
        qpdf)
            repo_url="https://github.com/qpdf/qpdf.git"
            repo_tag="v11.9.1"
            cmake_flags+=("-DBUILD_STATIC_LIBS=OFF" "-DBUILD_DOC=OFF" "-DREQUIRE_CRYPTO_OPENSSL=ON" "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -march=native -funroll-loops -falign-loops=32 -flto=auto -fomit-frame-pointer -pipe -include cstdint")
            ;;
        libarchive|archive|engine-libarchive)
            repo_url="https://github.com/libarchive/libarchive.git"
            repo_tag="v3.7.4"
            cmake_flags+=("-DENABLE_TEST=OFF" "-DENABLE_TAR=OFF" "-DENABLE_CPIO=OFF" "-DENABLE_CAT=OFF" "-DENABLE_UNZIP=OFF" "-DCMAKE_C_FLAGS_RELEASE=-O3 -march=native -mcrc32 -fno-strict-aliasing -funroll-loops -falign-loops=32 -flto=auto -fomit-frame-pointer -fno-semantic-interposition -pipe")
            ;;
        poppler|engine-poppler)
            repo_url="https://gitlab.freedesktop.org/poppler/poppler.git"
            repo_tag="poppler-26.08.0"
            cmake_flags+=("-DENABLE_QT6=ON" "-DENABLE_QT5=OFF" "-DENABLE_GLIB=OFF" "-DENABLE_CPP=OFF" "-DENABLE_BOOST=OFF" "-DENABLE_UTILS=ON" "-DENABLE_NSS3=OFF" "-DENABLE_GPGME=OFF" "-DENABLE_LIBCURL=OFF" "-DBUILD_QT6_TESTS=OFF" "-DBUILD_CPP_TESTS=OFF" "-DBUILD_MANUAL_TESTS=OFF" "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -march=native -mcrc32 -funroll-loops -falign-loops=32 -flto=auto -fomit-frame-pointer -fno-semantic-interposition -pipe -include cstdint")
            ;;
        7zip|7z|7zz)
            repo_url="https://github.com/ip7z/7zip.git"
            repo_tag="24.08"
            ;;
        zstd)
            repo_url="https://github.com/facebook/zstd.git"
            repo_tag="v1.5.6"
            cmake_flags+=("-DZSTD_BUILD_TESTS=OFF" "-DZSTD_BUILD_PROGRAMS=ON" "-DCMAKE_C_FLAGS_RELEASE=-O3 -march=native -funroll-loops -falign-loops=32 -flto=auto -fomit-frame-pointer -pipe")
            ;;
        xz|liblzma)
            repo_url="https://github.com/tukaani-project/xz.git"
            repo_tag="v5.6.3"
            cmake_flags+=("-DBUILD_TESTING=OFF" "-DCMAKE_C_FLAGS_RELEASE=-O3 -march=native -funroll-loops -falign-loops=32 -flto=auto -fomit-frame-pointer -pipe")
            ;;
        poppler-data)
            echo -e "${BLUE}${BOLD}[*] Scaricamento tabelle font asiatici CJK (poppler-data) in ${HOME}/.local/share/poppler (Zero Sudo, Zero Compilazione)...${NC}"
            local data_src="${SCRIPT_DIR}/build_deps/poppler-data"
            mkdir -p "${SCRIPT_DIR}/build_deps" "${HOME}/.local/share/poppler"
            if [ ! -d "$data_src/.git" ]; then
                git clone --depth 1 -b "poppler-data-0.4.12" "https://gitlab.freedesktop.org/poppler/poppler-data.git" "$data_src" 2>/dev/null || \
                git clone --depth 1 "https://gitlab.freedesktop.org/poppler/poppler-data.git" "$data_src"
            else
                (cd "$data_src" && git pull --ff-only 2>/dev/null || true)
            fi
            for sub in cMap cidToUnicode nameToUnicode unicodeMap; do
                if [ -d "$data_src/$sub" ]; then
                    rm -rf "${HOME}/.local/share/poppler/$sub"
                    cp -r "$data_src/$sub" "${HOME}/.local/share/poppler/$sub"
                fi
            done
            echo -e "${GREEN}${BOLD}[✓] poppler-data installato con successo in ${HOME}/.local/share/poppler!${NC}"
            exit 0
            ;;
        tessdata|tesseract-ocr-ita|ocr-ita)
            echo -e "${BLUE}${BOLD}[*] Scaricamento modelli neurali OCR (tessdata_fast) in ${HOME}/.local/share (Zero Sudo, Zero Compilazione)...${NC}"
            local tess_dir="${HOME}/.local/share/bubezview/tessdata"
            local tess_xdg="${HOME}/.local/share/tessdata"
            mkdir -p "$tess_dir" "$tess_xdg"
            for lang in ita eng osd; do
                echo -e "${BLUE}    Download ${lang}.traineddata...${NC}"
                curl -fL "https://raw.githubusercontent.com/tesseract-ocr/tessdata_fast/main/${lang}.traineddata" -o "${tess_dir}/${lang}.traineddata"
                ln -sf "${tess_dir}/${lang}.traineddata" "${tess_xdg}/${lang}.traineddata" 2>/dev/null || cp -f "${tess_dir}/${lang}.traineddata" "${tess_xdg}/${lang}.traineddata"
            done
            echo -e "${GREEN}${BOLD}[✓] Modelli OCR installati in ${tess_dir}!${NC}"
            exit 0
            ;;
        mpv|engine-mpv|media)
            echo -e "${BLUE}${BOLD}[*] Scaricamento binario portabile MPV AppImage in ${HOME}/.local/bin/mpv (Zero Sudo)...${NC}"
            local arch
            arch=$(uname -m)
            local mpv_url=""
            mpv_url=$(curl -s "https://api.github.com/repos/pkgforge-dev/mpv-AppImage/releases/latest" | grep "browser_download_url" | grep "anylinux-${arch}\.AppImage\"" | head -n 1 | cut -d'"' -f4 || true)
            if [ -z "$mpv_url" ]; then
                echo -e "${RED}${BOLD}[!] Impossibile trovare il pacchetto MPV AppImage per architettura ${arch}.${NC}" >&2
                exit 1
            fi
            mkdir -p "${HOME}/.local/bin"
            echo -e "${BLUE}    Sorgente: ${mpv_url}${NC}"
            curl -fL --progress-bar "$mpv_url" -o "${HOME}/.local/bin/mpv"
            chmod +x "${HOME}/.local/bin/mpv"
            echo -e "${GREEN}${BOLD}[✓] Eseguibile portabile 'mpv' installato in ${HOME}/.local/bin/mpv!${NC}"
            exit 0
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
            echo -e "${RED}${BOLD}[!] Errore: Il componente '${comp}' non ha una ricetta di installazione user-space.${NC}"
            echo -e "${RED}    Componenti supportati: poppler, poppler-data, libarchive, 7zip, qpdf, tesseract, tesseract-ocr-ita, zstd, xz, mpv, image-formats.${NC}"
            exit 1
            ;;
    esac

    # Applica override personalizzati se specificati
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

    echo -e "${BLUE}${BOLD}[*] Compilazione ${comp} (${repo_tag}) da sorgente per l'ambiente utente (${HOME}/.local)...${NC}"
    echo -e "${BLUE}    Sorgente: ${repo_url}${NC}"
    if [ -n "$custom_flags" ]; then
        echo -e "${BLUE}    Flag extra: ${custom_flags}${NC}"
    fi

    local COMP_SRC="${SCRIPT_DIR}/build_deps/${comp}"
    local COMP_BUILD="${SCRIPT_DIR}/build_deps/${comp}_build"
    mkdir -p "${SCRIPT_DIR}/build_deps"
    if [ ! -d "$COMP_SRC/.git" ]; then
        if ! git clone --depth 1 -b "$repo_tag" "$repo_url" "$COMP_SRC" 2>/dev/null; then
            echo -e "${YELLOW}[!] Tag '${repo_tag}' non trovato, fallback su ramo principale...${NC}"
            git clone --depth 1 -b "6.8" "$repo_url" "$COMP_SRC" 2>/dev/null || git clone --depth 1 "$repo_url" "$COMP_SRC"
        fi
    else
        (cd "$COMP_SRC" && git fetch --depth 1 origin "$repo_tag" 2>/dev/null || true && git checkout "$repo_tag" 2>/dev/null || true)
    fi
    local cpu_cores_sub
    cpu_cores_sub=$(nproc 2>/dev/null || echo "4")

    if [ "$comp" = "7zip" ] || [ "$comp" = "7z" ] || [ "$comp" = "7zz" ]; then
        local flags_7z="-O3 -march=native -mcrc32 -funroll-loops -falign-loops=32 -flto -fomit-frame-pointer -pipe"
        if [ -n "$custom_flags" ]; then
            flags_7z="$custom_flags"
        fi
        echo -e "${BLUE}${BOLD}[*] Compilazione 7-Zip (7zz) con makefile.gcc (${cpu_cores_sub} thread)...${NC}"
        (cd "${COMP_SRC}/CPP/7zip/Bundles/Alone2" && make -j"${cpu_cores_sub}" -f makefile.gcc LOCAL_FLAGS="${flags_7z}")
        mkdir -p "${HOME}/.local/bin"
        install -m 755 "${COMP_SRC}/CPP/7zip/Bundles/Alone2/_o/7zz" "${HOME}/.local/bin/7zz"
        echo -e "${GREEN}${BOLD}[✓] Componente '7zz' installato con successo in ${HOME}/.local/bin/7zz!${NC}"
        exit 0
    fi

    local COMP_CMAKE_SRC="$COMP_SRC"
    if [ "$comp" = "zstd" ]; then
        COMP_CMAKE_SRC="${COMP_SRC}/build/cmake"
    fi

    mkdir -p "$COMP_BUILD"
    if ! cmake -B "$COMP_BUILD" -S "$COMP_CMAKE_SRC" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="${HOME}/.local" -DCMAKE_INSTALL_RPATH='$ORIGIN/../lib:$ORIGIN/../lib64' -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=TRUE -DCMAKE_BUILD_WITH_INSTALL_RPATH=TRUE "${cmake_flags[@]}"; then
        echo -e "${YELLOW}[!] Configurazione CMake fallita; pulizia cache e nuovo tentativo...${NC}"
        rm -rf "${COMP_BUILD}"
        mkdir -p "${COMP_BUILD}"
        cmake -B "$COMP_BUILD" -S "$COMP_CMAKE_SRC" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="${HOME}/.local" -DCMAKE_INSTALL_RPATH='$ORIGIN/../lib:$ORIGIN/../lib64' -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=TRUE -DCMAKE_BUILD_WITH_INSTALL_RPATH=TRUE "${cmake_flags[@]}"
    fi
    cmake --build "$COMP_BUILD" --parallel "$cpu_cores_sub"
    cmake --install "$COMP_BUILD"
    echo -e "${GREEN}${BOLD}[✓] Componente '${comp}' installato con successo in ${HOME}/.local!${NC}"
    exit 0
}

# ------------------------------------------------------------------------------
# 5. GESTIONE OPERAZIONI (Clean, Reinstall, Uninstall)
# ------------------------------------------------------------------------------
do_clean() {
    local silent="${1:-0}"
    echo -e "${BLUE}${BOLD}[*] Pulizia file temporanei di compilazione in corso...${NC}"

    local dirs_to_clean=(
        "${SCRIPT_DIR}/build_install"
        "${SCRIPT_DIR}/build_deb"
        "${SCRIPT_DIR}/build_fedora"
        "${SCRIPT_DIR}/build"
    )

    local bytes_freed=0
    for d in "${dirs_to_clean[@]}"; do
        if [ -d "$d" ]; then
            local sz
            sz=$(du -sb "$d" 2>/dev/null | awk '{print $1}')
            sz=${sz:-0}
            bytes_freed=$((bytes_freed + sz))
            echo -e "${BLUE}    Rimozione: ${d}${NC}"
            if ! rm -rf "$d" 2>/dev/null; then
                if command -v docker >/dev/null 2>&1 && docker ps -q --filter "name=debian" 2>/dev/null | grep -q .; then
                    docker exec debian rm -rf "$d" 2>/dev/null || true
                fi
                rm -rf "$d" 2>/dev/null || true
            fi
        fi
    done

    # Pulisci anche le cartelle di build intermedie di build_deps (senza toccare i sorgenti git)
    find "${SCRIPT_DIR}/build_deps" -maxdepth 1 -type d -name "*_build" -exec rm -rf {} + 2>/dev/null || true
    find "${SCRIPT_DIR}" -maxdepth 1 -type f -name "*.log" -exec rm -f {} + 2>/dev/null || true

    local mb_freed=$((bytes_freed / 1024 / 1024))
    echo -e "${GREEN}${BOLD}[✓] Pulizia completata! Liberati circa ${mb_freed} MB di cache di build.${NC}"
    if [ "$silent" -eq 0 ] && [ "$UI_MODE" != "cli" ] && [ "$FORCE_CLI" -eq 0 ]; then
        ui_info "Pulizia Completata" "Tutte le cartelle di build temporanee sono state rimosse.\nSpazio liberato: ~${mb_freed} MB."
    fi
}

do_reinstall() {
    echo -e "${BLUE}${BOLD}[*] Avvio procedura di reinstallazione pulita da zero...${NC}"
    # 1. Pulisci la cache di build
    do_clean 1

    # 2. Reset configurazione e cache per primo avvio pulito (Wizard)
    echo -e "${YELLOW}[*] Reset configurazione, cache e compilazioni locali per riavviare il Setup di BubezView...${NC}"
    rm -rf "$HOME/.config/Bubez" 2>/dev/null || true
    rm -rf "$HOME/.config/bubezview" 2>/dev/null || true
    rm -rf "$HOME/.config/bubezview-dev" 2>/dev/null || true
    rm -rf "$HOME/.cache/Bubez" 2>/dev/null || true
    rm -rf "$HOME/.cache/BubezView" 2>/dev/null || true
    rm -rf "$HOME/.cache/bubezview" 2>/dev/null || true
    rm -rf "$HOME/.local/share/Bubez" 2>/dev/null || true
    rm -f "$HOME/.local/state/BubezViewstaterc" 2>/dev/null || true
    rm -rf "$HOME/.local/share/$APP_ID" 2>/dev/null || true
    rm -rf "$HOME/.local/lib/$APP_ID" 2>/dev/null || true
    rm -f "$HOME/.local/bin/$APP_ID" 2>/dev/null || true
    rm -f "$HOME/.local/bin/qpdf" "$HOME/.local/bin/pdfimages" "$HOME/.local/bin/pdfunite" \
          "$HOME/.local/bin/tesseract" "$HOME/.local/bin/7zz" "$HOME/.local/bin/7z" \
          "$HOME/.local/bin/zstd" "$HOME/.local/bin/xz" "$HOME/.local/bin/mpv" 2>/dev/null || true
    rm -rf "$HOME/.local/share/poppler" "$HOME/.local/share/tessdata" 2>/dev/null || true
    rm -f "$HOME/.local/lib/libpoppler"* "$HOME/.local/lib/libarchive"* \
          "$HOME/.local/lib/libqpdf"* "$HOME/.local/lib/libtesseract"* \
          "$HOME/.local/lib/libzstd"* "$HOME/.local/lib/liblzma"* 2>/dev/null || true
    rm -rf "${SCRIPT_DIR}/build_deps"/*_build 2>/dev/null || true
    rm -rf /tmp/${APP_ID}_installer_* /tmp/${APP_ID}*.log 2>/dev/null || true

    IS_UPDATE=0
    ACTION=""
}

do_uninstall() {
    if [ "$IS_UPDATE" -eq 0 ]; then
        if ! ui_ask_yesno "Disinstallazione BubezView" "Sei sicuro di voler disinstallare BubezView dal tuo sistema?"; then
            echo -e "${YELLOW}[*] Disinstallazione annullata dall'utente.${NC}"
            exit 0
        fi
        if [ "$PURGE" -eq 0 ]; then
            if ui_ask_yesno "Cancella tutto" "Vuoi eliminare anche configurazioni, cache, moduli .so e componenti compilati localmente (~/.config/Bubez, ~/.cache/BubezView, ~/.local/lib/bubezview, ecc.)?"; then
                PURGE=1
            fi
        fi
    fi

    echo -e "${BLUE}${BOLD}[*] Disinstallazione di $APP_NAME in corso...${NC}"

    # 0. Ferma eventuali istanze di BubezView in standby/background prima di rimuovere i file
    pkill -x "$APP_ID" 2>/dev/null || true
    sleep 0.3

    # 1. Rimuovi binario, plugin .so e cartelle share/lib
    rm -f "$HOME/.local/bin/$APP_ID"
    rm -rf "$HOME/.local/share/$APP_ID"
    rm -rf "$HOME/.local/lib/$APP_ID"
    rm -rf "$HOME/.local/lib/x86_64-linux-gnu/$APP_ID"
    rm -rf "$HOME/.local/lib64/$APP_ID"

    # 2. Rimuovi lanciatori .desktop (standard e sviluppo)
    rm -f "$HOME/.local/share/applications/$APP_ID.desktop"
    rm -f "$HOME/.local/share/applications/${APP_ID}_sviluppo.desktop"

    # 3. Rimuovi icone
    rm -f "$HOME/.local/share/icons/hicolor/scalable/apps/$APP_ID".*
    rm -f "$HOME/.local/share/icons/hicolor/256x256/apps/$APP_ID".*

    # 4. Rimuovi integrazioni file manager (Dolphin ServiceMenu, legacy KDE5, Nemo e Nautilus)
    rm -f "$HOME/.local/share/kio/servicemenus/$APP_ID.desktop"
    rm -f "$HOME/.local/share/kio/servicemenus/${APP_ID}_sviluppo.desktop"
    rm -f "$HOME/.local/share/kservices5/ServiceMenus/$APP_ID.desktop"
    rm -f "$HOME/.local/share/kservices5/ServiceMenus/${APP_ID}_sviluppo.desktop"
    rm -f "$HOME/.local/share/nemo/actions/$APP_ID.nemo_action"
    rm -f "$HOME/.local/share/nemo/actions/${APP_ID}_sviluppo.nemo_action"
    rm -f "$HOME/.local/share/nautilus/scripts/Preview with BubezView"
    rm -f "$HOME/.local/share/nautilus/scripts/Anteprima con BubezView"*
    for rc in "$HOME/.local/share/kxmlgui5/dolphin/dolphinui.rc" \
              "$HOME/.local/share/kxmlgui6/dolphin/dolphinui.rc"; do
        if [ -f "$rc" ]; then
            sed -i '/servicemenu_bubezview/d; /toggle_selection_mode/d' "$rc" 2>/dev/null || true
        fi
    done
    for f in "$HOME/.local/share/dbus-1/services/org.gnome.NautilusPreviewer.service" \
             "$HOME/.local/share/dbus-1/services/org.nemo.Preview.service" \
             "$HOME/.local/share/dbus-1/services/org.nemo.Previewer.service" \
             "$HOME/.local/bin/sushi" \
             "$HOME/.local/bin/nemo-preview"; do
        if [ -f "$f" ] && grep -qi "bubezview" "$f" 2>/dev/null; then
            rm -f "$f"
        fi
    done

    # 5. Se PURGE=1, rimuovi anche configurazione, dati Qt, cache e componenti compilati in ~/.local
    if [ "$PURGE" -eq 1 ]; then
        echo -e "${YELLOW}[*] Rimozione completa configurazione, dati Qt, cache e motori locali...${NC}"
        rm -rf "$HOME/.config/Bubez"
        rm -rf "$HOME/.config/bubezview"
        rm -rf "$HOME/.config/bubezview-dev"
        rm -rf "$HOME/.cache/Bubez"
        rm -rf "$HOME/.cache/BubezView"
        rm -rf "$HOME/.cache/bubezview"
        rm -rf "$HOME/.local/share/Bubez"
        rm -f "$HOME/.local/state/BubezViewstaterc"
        rm -f "$HOME/.local/bin/pdfimages" "$HOME/.local/bin/pdfunite" "$HOME/.local/bin/pdftoppm" \
              "$HOME/.local/bin/pdftotext" "$HOME/.local/bin/pdfinfo" "$HOME/.local/bin/pdffonts" \
              "$HOME/.local/bin/qpdf" "$HOME/.local/bin/tesseract" "$HOME/.local/bin/7zz" \
              "$HOME/.local/bin/7z" "$HOME/.local/bin/7za" "$HOME/.local/bin/zstd" \
              "$HOME/.local/bin/xz" "$HOME/.local/bin/mpv" "$HOME/.local/bin/bsdtar" \
              "$HOME/.local/bin/bsdunzip" 2>/dev/null || true
        rm -rf "$HOME/.local/share/poppler" "$HOME/.local/share/tessdata" 2>/dev/null || true
        rm -f "$HOME/.local/lib/libpoppler"* "$HOME/.local/lib/libarchive"* \
              "$HOME/.local/lib/libqpdf"* "$HOME/.local/lib/libtesseract"* \
              "$HOME/.local/lib/libzstd"* "$HOME/.local/lib/liblzma"* \
              "$HOME/.local/lib64/libpoppler"* "$HOME/.local/lib64/libarchive"* \
              "$HOME/.local/lib64/libqpdf"* "$HOME/.local/lib64/libtesseract"* \
              "$HOME/.local/lib64/libzstd"* "$HOME/.local/lib64/liblzma"* 2>/dev/null || true
        rm -rf /tmp/${APP_ID}_installer_* /tmp/${APP_ID}*.log 2>/dev/null || true
    fi

    # 6. Aggiorna database di sistema
    command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
    command -v kbuildsycoca6 >/dev/null 2>&1 && kbuildsycoca6 --noincremental 2>/dev/null || true
    command -v kbuildsycoca5 >/dev/null 2>&1 && kbuildsycoca5 --noincremental 2>/dev/null || true
    command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -f -t "$HOME/.local/share/icons/hicolor" 2>/dev/null || true

    echo -e "${GREEN}${BOLD}[✓] $APP_NAME è stato disinstallato con successo dal sistema.${NC}"
    if [ "$UI_MODE" != "cli" ] && [ "$FORCE_CLI" -eq 0 ]; then
        ui_info "Disinstallazione Completata" "$APP_NAME è stato disinstallato con successo dal tuo sistema."
    fi
    exit 0
}

show_interactive_menu() {
    local choice=""
    if [ "$UI_MODE" = "zenity" ]; then
        choice=$(zenity --list --title="BubezView - Gestione Installazione" \
            --text="Scegli l'operazione da eseguire per BubezView:" \
            --radiolist --column="" --column="Codice" --column="Operazione" \
            --hide-column=2 --width=480 --height=240 \
            TRUE "install" "🚀  Installa o aggiorna BubezView" \
            FALSE "reinstall" "🔄  Reinstalla da zero (Clean Reinstall & Reset Setup)" \
            FALSE "uninstall" "🗑️   Disinstalla BubezView dal sistema" \
            2>/dev/null || true)
    elif [ "$UI_MODE" = "kdialog" ]; then
        choice=$(kdialog --title "BubezView - Gestione Installazione" \
            --radiolist "Scegli l'operazione da eseguire per BubezView:" \
            "install" "🚀 Installa o aggiorna BubezView" on \
            "reinstall" "🔄 Reinstalla da zero (Clean Reinstall & Reset Setup)" off \
            "uninstall" "🗑️ Disinstalla BubezView dal sistema" off \
            2>/dev/null || true)
    elif [ "$UI_MODE" = "yad" ]; then
        choice=$(yad --list --title="BubezView - Gestione Installazione" \
            --text="Scegli l'operazione da eseguire:" \
            --radiolist --column="" --column="Codice" --column="Operazione" \
            --hide-column=2 --width=480 --height=240 \
            TRUE "install" "🚀 Installa o aggiorna BubezView" \
            FALSE "reinstall" "🔄 Reinstalla da zero (Clean Reinstall & Reset Setup)" \
            FALSE "uninstall" "🗑️ Disinstalla BubezView dal sistema" \
            2>/dev/null || true)
    fi

    case "$choice" in
        install)
            # Procedi direttamente con l'installazione
            ;;
        reinstall)
            do_reinstall
            ;;
        uninstall)
            do_uninstall
            exit 0
            ;;
        *)
            echo -e "${YELLOW}[*] Operazione annullata.${NC}"
            exit 0
            ;;
    esac
}

# ------------------------------------------------------------------------------
# 6. ESECUZIONE PRINCIPALE
# ------------------------------------------------------------------------------
main() {
    # 1. Gestione azioni immediate da CLI
    if [ "$ACTION" = "uninstall" ]; then
        do_uninstall
        exit 0
    elif [ "$ACTION" = "clean" ]; then
        do_clean
        exit 0
    elif [ "$ACTION" = "reinstall" ]; then
        do_reinstall
    elif [ "$ACTION" = "build_component" ] && [ -n "$TARGET_COMPONENT" ]; then
        case "$TARGET_COMPONENT" in
            poppler-data|tessdata|tesseract-ocr-ita|ocr-ita|mpv|engine-mpv|media)
                ;;
            *)
                check_build_tools
                ;;
        esac
        build_source_component "$TARGET_COMPONENT" "$CUSTOM_GIT_URL" "$CUSTOM_GIT_BRANCH" "$CUSTOM_BUILD_FLAGS"
        exit 0
    fi

    # 2. Se avviato interattivamente senza argomenti, mostra il menu a 3 scelte
    if [ "$FORCE_CLI" -eq 0 ] && [ "$UI_MODE" != "cli" ] && [ -z "$ACTION" ]; then
        show_interactive_menu
    fi

    # 3. Controllo strumenti di compilazione
    check_build_tools

    local work_dir
    work_dir=$(mktemp -d -t "${APP_ID}_installer_XXXXXX")
    trap 'rm -rf "$work_dir"' EXIT

    local tar_file="$work_dir/source.tar.gz"
    local extract_dir="$work_dir/source"
    local src_cmakelists=""

    # Se lanciato dal repository locale, compila direttamente dai sorgenti
    if [ -f "$SCRIPT_DIR/CMakeLists.txt" ]; then
        src_cmakelists="$SCRIPT_DIR/CMakeLists.txt"
        extract_dir="$SCRIPT_DIR"
    else
        mkdir -p "$extract_dir"
        local info_json="$work_dir/versioni.json"
        curl -s -L "$PROJECT_URL" -o "$info_json" || {
            ui_error "Errore di Rete" "Impossibile scaricare l'indice delle versioni da $PROJECT_URL"
            exit 1
        }

        local drive_url=""
        if command -v python3 >/dev/null 2>&1; then
            drive_url=$(python3 -c "
import json, sys
try:
    with open('$info_json', 'r', encoding='utf-8') as f:
        data = json.load(f)
    app = data.get('$APP_ID', {})
    url = app.get('link_download_source') or app.get('link_download') or app.get('link_download_rolling') or ''
    print(url)
except Exception:
    pass
")
        elif command -v jq >/dev/null 2>&1; then
            drive_url=$(jq -r ".\"$APP_ID\".link_download_source // .\"$APP_ID\".link_download // .\"$APP_ID\".link_download_rolling // empty" "$info_json" 2>/dev/null || true)
        fi

        if [ -z "$drive_url" ]; then
            drive_url=$(grep -A 60 "\"$APP_ID\"" "$info_json" | grep -m 1 -E '"link_download_source"|"link_download"|"link_download_rolling"' | cut -d'"' -f4 || true)
        fi

        if [ -z "$drive_url" ]; then
            ui_error "Errore" "Link di download per '$APP_ID' non trovato nel catalogo."
            exit 1
        fi

        download_source_payload "$drive_url" "$tar_file"

        # Verifica integrità e firma crittografica Ed25519
        if [ ! -f "$tar_file" ] || [ ! -s "$tar_file" ]; then
            ui_error "Errore Download" "Il file scaricato non esiste o è vuoto (0 byte)."
            rm -f "$tar_file"
            exit 1
        fi

        local expected_sha256=""
        local signature_b64=""

        if command -v python3 >/dev/null 2>&1; then
            eval "$(python3 -c "
import json
try:
    with open('$info_json', 'r', encoding='utf-8') as f:
        data = json.load(f)
    app = data.get('$APP_ID', {})
    sha = app.get('sha256', '')
    sig = app.get('signature', '')
    print(f'expected_sha256=\"{sha}\"')
    print(f'signature_b64=\"{sig}\"')
except Exception:
    pass
")"
        elif command -v jq >/dev/null 2>&1; then
            expected_sha256=$(jq -r ".\"$APP_ID\".sha256 // empty" "$info_json" 2>/dev/null || true)
            signature_b64=$(jq -r ".\"$APP_ID\".signature // empty" "$info_json" 2>/dev/null || true)
        fi

        if [ -z "$expected_sha256" ]; then
            expected_sha256=$(grep -A 40 "\"$APP_ID\"" "$info_json" | grep -m 1 '"sha256"' | cut -d'"' -f4 || true)
        fi
        if [ -z "$signature_b64" ]; then
            signature_b64=$(grep -A 40 "\"$APP_ID\"" "$info_json" | grep -m 1 '"signature"' | cut -d'"' -f4 || true)
        fi

        local actual_sha256=""
        if command -v sha256sum >/dev/null 2>&1; then
            actual_sha256=$(sha256sum "$tar_file" | awk '{print $1}')
        elif command -v shasum >/dev/null 2>&1; then
            actual_sha256=$(shasum -a 256 "$tar_file" | awk '{print $1}')
        elif command -v openssl >/dev/null 2>&1; then
            actual_sha256=$(openssl dgst -sha256 "$tar_file" | awk '{print $NF}')
        fi

        if [ -n "$expected_sha256" ]; then
            if [ -z "$actual_sha256" ] || [ "$actual_sha256" != "$expected_sha256" ]; then
                rm -f "$tar_file"
                ui_error "Errore Integrità" "L'impronta SHA-256 del file scaricato non corrisponde! Possibile download corrotto."
                exit 1
            fi
            echo -e "${GREEN}[✓] Impronta SHA-256 verificata con successo:${NC} $actual_sha256"
        fi

        if [ -n "$signature_b64" ]; then
            if [ -z "$RELEASE_PUBKEY" ]; then
                rm -f "$tar_file"
                ui_error "Errore Sicurezza" "Firma crittografica presente ma Chiave Pubblica non configurata nell'installer."
                exit 1
            fi

            local pubkey_file="$work_dir/release_pubkey.pem"
            local hash_file="$work_dir/release_hash.txt"
            local sig_file="$work_dir/release_sig.bin"

            printf "%s\n" "$RELEASE_PUBKEY" > "$pubkey_file"
            printf "%s" "$actual_sha256" > "$hash_file"

            if ! (echo "$signature_b64" | base64 -d > "$sig_file" 2>/dev/null); then
                rm -f "$tar_file" "$sig_file" "$hash_file" "$pubkey_file"
                ui_error "Errore Sicurezza" "Firma crittografica presente ma non decodificabile (Base64 invalido)."
                exit 1
            fi

            local sig_valid=0

            if command -v openssl >/dev/null 2>&1; then
                if openssl pkeyutl -verify -rawin -pubin -inkey "$pubkey_file" -in "$hash_file" -sigfile "$sig_file" >/dev/null 2>&1; then
                    sig_valid=1
                fi
            fi

            if [ "$sig_valid" -eq 0 ] && command -v python3 >/dev/null 2>&1; then
                if python3 -c "
import sys
try:
    from cryptography.hazmat.primitives import serialization
    with open('$pubkey_file', 'rb') as f:
        pub = serialization.load_pem_public_key(f.read())
    with open('$hash_file', 'rb') as f:
        data = f.read()
    with open('$sig_file', 'rb') as f:
        sig = f.read()
    pub.verify(sig, data)
    sys.exit(0)
except Exception:
    sys.exit(1)
" >/dev/null 2>&1; then
                    sig_valid=1
                fi
            fi

            rm -f "$sig_file" "$hash_file" "$pubkey_file"

            if [ "$sig_valid" -ne 1 ]; then
                rm -f "$tar_file"
                ui_error "Errore Sicurezza" "Verifica della firma crittografica Ed25519 FALLITA! Il pacchetto potrebbe essere stato manomesso."
                exit 1
            fi

            echo -e "${GREEN}[✓] Firma crittografica Ed25519 AUTENTICATA con successo!${NC}"
        fi

        if [ "$UI_MODE" = "zenity" ]; then
            (
                tar -xzf "$tar_file" -C "$extract_dir" --strip-components=1 2>&1 || \
                tar -xzf "$tar_file" -C "$extract_dir" 2>&1
            ) | zenity --progress --title="Estrazione - $APP_NAME" --text="Estrazione dei file in corso..." --pulsate --auto-close 2>/dev/null || true
        else
            echo -e "${BLUE}[*] Estrazione archivio sorgente...${NC}"
            tar -xzf "$tar_file" -C "$extract_dir" --strip-components=1 2>/dev/null || \
            tar -xzf "$tar_file" -C "$extract_dir" 2>/dev/null
        fi

        src_cmakelists="$extract_dir/CMakeLists.txt"
        if [ ! -f "$src_cmakelists" ]; then
            src_cmakelists=$(find "$extract_dir" -maxdepth 3 -name "CMakeLists.txt" | head -n 1 || true)
            if [ -n "$src_cmakelists" ]; then
                extract_dir="$(dirname "$src_cmakelists")"
            fi
        fi
    fi

    if [ ! -f "$src_cmakelists" ]; then
        ui_error "Errore Compilazione" "Nessun file CMakeLists.txt trovato nei sorgenti dell'applicazione."
        exit 1
    fi

    local build_dir="$extract_dir/build_install"
    mkdir -p "$build_dir"

    local make_prog=""
    if command -v make >/dev/null 2>&1; then
        make_prog=$(command -v make)
    elif command -v gmake >/dev/null 2>&1; then
        make_prog=$(command -v gmake)
    fi
    local make_flag=()
    if [ -n "$make_prog" ]; then
        make_flag=("-DCMAKE_MAKE_PROGRAM=$make_prog")
    fi

    local build_log="$work_dir/${APP_ID}_build.log"
    rm -f "$build_log" 2>/dev/null || true

    # Selective modular .so plugin flags:
    # Always pass explicit ON/OFF for every module so stale CMakeCache.txt values
    # can never silently re-enable an uninstalled plugin.
    local flag_folder="OFF"
    local flag_pdf="OFF"
    local flag_audio="OFF"
    local flag_video="OFF"
    local flag_image="OFF"

    if [ -n "$BUILD_ONLY_PLUGIN" ]; then
        case "$BUILD_ONLY_PLUGIN" in
            folder)      flag_folder="ON" ;;
            pdf)         flag_pdf="ON" ;;
            audio|media) BUILD_ONLY_PLUGIN="audio"; flag_audio="ON" ;;
            video)       flag_video="ON" ;;
            image)       flag_image="ON" ;;
            *)
                echo -e "${RED}[!] Modulo plugin sconosciuto: $BUILD_ONLY_PLUGIN${NC}"
                exit 1
                ;;
        esac
    else
        local check_plugins_dirs=("$HOME/.local/share/$APP_ID/plugins" "$HOME/.local/lib/$APP_ID/plugins")
        for pdir in "${check_plugins_dirs[@]}"; do
            [ -f "$pdir/bubez_folder.so" ] && flag_folder="ON"
            [ -f "$pdir/bubez_pdf.so" ]    && flag_pdf="ON"
            [ -f "$pdir/bubez_audio.so" ]  && flag_audio="ON"
            [ -f "$pdir/bubez_video.so" ]  && flag_video="ON"
            [ -f "$pdir/bubez_image.so" ]  && flag_image="ON"
        done
    fi

    local plugin_cmake_flags=(
        "-DBUILD_ALL_PLUGINS=OFF"
        "-DBUILD_PLUGIN_MEDIA=OFF"
        "-DBUILD_PLUGIN_FOLDER=${flag_folder}"
        "-DBUILD_PLUGIN_PDF=${flag_pdf}"
        "-DBUILD_PLUGIN_AUDIO=${flag_audio}"
        "-DBUILD_PLUGIN_VIDEO=${flag_video}"
        "-DBUILD_PLUGIN_IMAGE=${flag_image}"
    )

    # Configurazione CMake
    local cfg_ret=0
    local cfg_status_file="$work_dir/cfg_status"

    while true; do
        rm -f "$cfg_status_file" 2>/dev/null || true
        if [ "$UI_MODE" = "zenity" ]; then
            (
                echo "# Configurazione del progetto con CMake..."
                cmake -S "$extract_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF "${make_flag[@]}" "${plugin_cmake_flags[@]}" "${EXTRA_CMAKE_ARGS[@]}" >> "$build_log" 2>&1
                echo "$?" > "$cfg_status_file"
            ) | zenity --progress --title="CMake Configure - $APP_NAME" --text="Preparazione della configurazione..." --pulsate --auto-close 2>/dev/null || true
            cfg_ret=$(cat "$cfg_status_file" 2>/dev/null || echo "1")
        else
            echo -e "${BLUE}[*] Esecuzione configurazione CMake...${NC}"
            cmake -S "$extract_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF "${make_flag[@]}" "${plugin_cmake_flags[@]}" "${EXTRA_CMAKE_ARGS[@]}" 2>&1 | tee -a "$build_log"
            cfg_ret=${PIPESTATUS[0]}
        fi

        if [ "$cfg_ret" -eq 0 ] && [ -f "$build_dir/CMakeCache.txt" ]; then
            break
        fi

        local missing_comps
        missing_comps=$(grep -oE 'Could NOT find [A-Za-z0-9_+-]+|Failed to find required Qt component "[^"]+"' "$build_log" 2>/dev/null | sed -E 's/Could NOT find //; s/Failed to find required Qt component "([^"]+)"/Qt6\1/' | sort -u | tr '\n' ' ' | sed 's/ *$//')
        local short_msg
        if [ -n "$missing_comps" ]; then
            short_msg="Mancano questi pacchetti/componenti:\n<b>$missing_comps</b>\n\nInstallali e poi premi <b>Riprova</b>.\n\nLog completo: $build_log"
        else
            short_msg="La configurazione di CMake è fallita.\nControlla il log e premi <b>Riprova</b>.\n\nLog: $build_log"
        fi

        if [ "$UI_MODE" = "zenity" ]; then
            if zenity --question --title="Dipendenze Mancanti - $APP_NAME" --text="$short_msg" --ok-label="Riprova" --cancel-label="Annulla" --width=460 2>/dev/null; then
                rm -rf "$build_dir/CMakeCache.txt" "$build_dir/CMakeFiles"
                continue
            fi
            exit 1
        else
            echo -e "${RED}[!] $short_msg${NC}"
            read -r -p "Premi INVIO per riprovare oppure Ctrl+C per annullare..."
            rm -rf "$build_dir/CMakeCache.txt" "$build_dir/CMakeFiles"
        fi
    done

    local cpu_cores
    cpu_cores=$(nproc 2>/dev/null || echo "2")

    # Fast single-plugin on-demand build path (--build-plugin <folder|pdf|media>)
    if [ -n "$BUILD_ONLY_PLUGIN" ]; then
        local target_name="bubez_${BUILD_ONLY_PLUGIN}"
        echo -e "${BLUE}[*] Compilazione modulo opzionale '${target_name}.so' (${cpu_cores} core)...${NC}"
        if ! cmake --build "$build_dir" --target "$target_name" --parallel "$cpu_cores" 2>&1 | tee -a "$build_log"; then
            echo -e "${RED}[!] Compilazione del modulo ${target_name}.so fallita. Log: $build_log${NC}"
            exit 1
        fi
        local built_so
        built_so=$(find "$build_dir/plugins" -type f -name "${target_name}.so" 2>/dev/null | head -n 1 || true)
        if [ -z "$built_so" ] || [ ! -f "$built_so" ]; then
            echo -e "${RED}[!] File ${target_name}.so non generato.${NC}"
            exit 1
        fi
        local plugins_dir="$HOME/.local/share/$APP_ID/plugins"
        mkdir -p "$plugins_dir"
        cp -f "$built_so" "$plugins_dir/${target_name}.so"
        echo -e "${GREEN}[✓] Modulo ${target_name}.so installato in ${plugins_dir}!${NC}"
        exit 0
    fi

    # Compilazione con tracciamento avanzamento reale per Zenity
    local build_ret=0
    local build_status_file="$work_dir/build_status"
    rm -f "$build_status_file" 2>/dev/null || true

    if [ "$UI_MODE" = "zenity" ]; then
        (
            cmake --build "$build_dir" --parallel "$cpu_cores" 2>&1 | while IFS= read -r line; do
                echo "$line" >> "$build_log"
                if [[ "$line" =~ \[[[:space:]]*([0-9]+)%\] ]]; then
                    echo "${BASH_REMATCH[1]}"
                    echo "# Compilazione in corso (${BASH_REMATCH[1]}%)..."
                fi
            done
            echo "${PIPESTATUS[0]}" > "$build_status_file"
        ) | zenity --progress --title="Compilazione - $APP_NAME" --text="Compilazione del codice sorgente C++ in corso..." --percentage=0 --auto-close 2>/dev/null || true
        build_ret=$(cat "$build_status_file" 2>/dev/null || echo "1")
    else
        echo -e "${BLUE}[*] Compilazione in corso ($cpu_cores core)...${NC}"
        cmake --build "$build_dir" --parallel "$cpu_cores" 2>&1 | tee -a "$build_log"
        build_ret=${PIPESTATUS[0]}
    fi

    local exe_path=""
    if [ -n "$TARGET_EXE" ] && [ -f "$build_dir/$TARGET_EXE" ]; then
        exe_path="$build_dir/$TARGET_EXE"
    else
        exe_path=$(find "$build_dir" -maxdepth 2 -type f -executable ! -name "*.sh" ! -name "cmake*" | head -n 1 || true)
    fi

    if [ "$build_ret" -ne 0 ] || [ -z "$exe_path" ] || [ ! -f "$exe_path" ]; then
        local err_tail
        err_tail=$(tail -n 25 "$build_log" 2>/dev/null || true)
        ui_error "Errore Compilazione" "Compilazione fallita. L'eseguibile non è stato generato.\nFile di log: $build_log\n\nUltime righe di errore:\n$err_tail"
        exit 1
    fi

    # --------------------------------------------------------------------------
    # INSTALLAZIONE FILE UTENTE (~/.local/share/bubezview & ~/.local/bin)
    # --------------------------------------------------------------------------
    local user_bin="$HOME/.local/bin"
    local user_apps="$HOME/.local/share/applications"
    local user_icons="$HOME/.local/share/icons/hicolor"
    local app_install_dir="$HOME/.local/share/$APP_ID"

    mkdir -p "$user_bin" "$user_apps" "$user_icons/scalable/apps" "$user_icons/256x256/apps" "$app_install_dir"

    local final_exe_name
    final_exe_name=$(basename "$exe_path")
    cp -f "$exe_path" "$app_install_dir/$final_exe_name"
    chmod +x "$app_install_dir/$final_exe_name"

    # Installa e sincronizza i plugin modulari (.so) nella cartella dedicata dell'applicazione
    local plugins_dir="$app_install_dir/plugins"
    mkdir -p "$plugins_dir"
    if [ -d "$build_dir/plugins" ]; then
        find "$build_dir/plugins" -type f -name "*.so" -exec cp -f {} "$plugins_dir/" \; 2>/dev/null || true
    fi

    # Assicura che ~/.local/bin/bubezview sia il symlink al binario installato
    ln -sf "$app_install_dir/$final_exe_name" "$user_bin/$APP_ID"

    # Salva una sola copia dell'installer autonomo per futuri aggiornamenti
    if [ -f "$0" ]; then
        cp -f "$0" "$app_install_dir/Install.sh" 2>/dev/null || true
        chmod +x "$app_install_dir/Install.sh" 2>/dev/null || true
    fi

    # Installazione icone ufficiali
    local found_icon=""
    if [ -f "$extract_dir/resources/icons/bubezview.png" ]; then
        found_icon="$extract_dir/resources/icons/bubezview.png"
    elif [ -f "$extract_dir/resources/icons/bubezview.svg" ]; then
        found_icon="$extract_dir/resources/icons/bubezview.svg"
    else
        found_icon=$(find "$extract_dir" -maxdepth 3 \( -name "bubezview.png" -o -name "bubezview.svg" \) 2>/dev/null | head -n 1 || true)
    fi

    if [ -n "$found_icon" ]; then
        local icon_ext="${found_icon##*.}"
        cp -f "$found_icon" "$app_install_dir/icon.$icon_ext"
        if [ "$icon_ext" = "svg" ]; then
            cp -f "$found_icon" "$user_icons/scalable/apps/$APP_ID.svg" 2>/dev/null || true
        else
            cp -f "$found_icon" "$user_icons/256x256/apps/$APP_ID.png" 2>/dev/null || true
            cp -f "$found_icon" "$user_icons/scalable/apps/$APP_ID.png" 2>/dev/null || true
        fi
        command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -f -t "$user_icons" 2>/dev/null || true
    fi

    # Installazione file .desktop ufficiale con tutti i MimeType associati
    local official_desktop="$extract_dir/resources/$APP_ID.desktop"
    if [ -f "$official_desktop" ]; then
        cp -f "$official_desktop" "$user_apps/$APP_ID.desktop"
        sed -i "s|^Exec=.*|Exec=$user_bin/$APP_ID %F|" "$user_apps/$APP_ID.desktop"
    else
        cat <<DESKTOP_EOF > "$user_apps/$APP_ID.desktop"
[Desktop Entry]
Type=Application
Name=$APP_NAME
Exec=$user_bin/$APP_ID %F
Icon=$APP_ID
Terminal=false
StartupWMClass=$APP_ID
Categories=${APP_CATEGORY:-Utility};
MimeType=image/png;image/jpeg;image/webp;image/svg+xml;image/bmp;image/gif;image/tiff;text/plain;text/markdown;application/pdf;
DESKTOP_EOF
    fi
    chmod +x "$user_apps/$APP_ID.desktop"

    # Aggiorna database desktop del sistema
    command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$user_apps" 2>/dev/null || true
    command -v kbuildsycoca6 >/dev/null 2>&1 && kbuildsycoca6 --noincremental 2>/dev/null || true
    command -v kbuildsycoca5 >/dev/null 2>&1 && kbuildsycoca5 --noincremental 2>/dev/null || true

    # --------------------------------------------------------------------------
    # VERIFICA FINALE (Smoke Test)
    # --------------------------------------------------------------------------
    local installed_bin="$app_install_dir/$final_exe_name"
    local test_failed=0
    local test_err_msg=""

    if command -v ldd >/dev/null 2>&1; then
        local missing_so
        missing_so=$(ldd "$installed_bin" 2>/dev/null | grep -i "not found" || true)
        if [ -n "$missing_so" ]; then
            test_failed=1
            test_err_msg="Librerie dinamiche mancanti riscontrate nel binario:\n$missing_so"
        fi
    fi

    if [ "$test_failed" -eq 0 ]; then
        if ! timeout 2s "$installed_bin" --version >/dev/null 2>&1 && \
           ! timeout 2s "$installed_bin" --help >/dev/null 2>&1 && \
           ! timeout 2s "$installed_bin" -h >/dev/null 2>&1; then
            local test_out
            test_out=$(timeout 1s "$installed_bin" 2>&1 || true)
            if [[ "$test_out" =~ "Illegal instruction" ]] || [[ "$test_out" =~ "Segmentation fault" ]]; then
                test_failed=1
                test_err_msg="Errore durante il test di avvio del binario:\n$test_out"
            fi
        fi
    fi

    if [ "$test_failed" -ne 0 ]; then
        ui_error "Verifica Installazione Fallita" "Il programma è stato compilato ma il test finale ha riscontrato un problema:\n\n$test_err_msg"
        exit 1
    fi

    echo -e "${GREEN}${BOLD}[✓] $APP_NAME installato con successo!${NC}"

    if [ "$IS_UPDATE" -eq 1 ]; then
        return 0
    fi

    # Avvia direttamente BubezView per procedere con il Setup / First Run Wizard
    echo -e "${BLUE}[*] Avvio di $APP_NAME...${NC}"
    "$user_bin/$APP_ID" >/dev/null 2>&1 &
}

main "$@"
