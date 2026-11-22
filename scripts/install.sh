#!/usr/bin/env bash
#
# Edition:
##  22/11/2026 by Tsukini
#
# File Name:
##  install.sh
#
# File Description:
##  Build and install xmouse on Linux / macOS (dependencies, libutils, binary, menu entry, uinput access, autostart)
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PREFIX="${PREFIX:-$HOME/.local}"
BUILD="$ROOT/build-release"
DEPS=0
UINPUT=0
AUTOSTART=0
GNOME=0
UNINSTALL=0
EXTENSION="xmouse@tsukini22.github.io"
EXTENSION_DIR="$HOME/.local/share/gnome-shell/extensions/$EXTENSION"
SUDO=""

usage() {
    cat << HELP
Usage: scripts/install.sh [options]
  --deps         install the build dependencies (dnf / apt / pacman / brew, asks for sudo) and libutils
  --uinput       let your user move / click / type for the other devices on Wayland (udev uaccess rule, sudo)
  --autostart    start xmouse at login (tray)
  --gnome        GNOME Wayland: the xmouse extension (edges without the portal dialog at every start, active after a new login)
  --prefix DIR   install folder (default: ~/.local -> ~/.local/bin/xmouse)
  --uninstall    remove xmouse from the prefix, the menu entry and the autostart
  -h, --help     this help
HELP
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --deps) DEPS=1 ;;
        --uinput) UINPUT=1 ;;
        --autostart) AUTOSTART=1 ;;
        --gnome) GNOME=1 ;;
        --prefix) PREFIX="$2"; shift ;;
        --uninstall) UNINSTALL=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "install.sh: unknown option $1" >&2; usage; exit 2 ;;
    esac
    shift
done
[[ "$(id -u)" -ne 0 ]] && command -v sudo > /dev/null && SUDO="sudo"

say() { printf '\033[1;34mxmouse\033[0m %s\n' "$*"; }
die() { printf '\033[1;31mxmouse\033[0m %s\n' "$*" >&2; exit 1; }

# ===== Uninstall =====
if [[ "$UNINSTALL" -eq 1 ]]; then
    rm -f "$PREFIX/bin/xmouse" "$HOME/.local/share/applications/xmouse.desktop" \
        "$HOME/.config/autostart/xmouse.desktop" "$HOME/.local/share/icons/hicolor/scalable/apps/xmouse.svg"
    if [[ -d "$EXTENSION_DIR" ]]; then
        gnome-extensions disable "$EXTENSION" 2> /dev/null || true
        rm -rf "$EXTENSION_DIR"
    fi
    say "removed (your settings, network and logs stay in ~/.config/xmouse and ~/.local/share/xmouse)"
    exit 0
fi

# ===== Dependencies =====
install_libutils() {
    if [[ -f /usr/include/utils/utils.hpp ]] || [[ -f /usr/local/include/utils/utils.hpp ]]; then return; fi
    say "installing libutils (TsukiNi22/libutils)"
    if command -v dnf > /dev/null || command -v apt-get > /dev/null; then
        curl -fsSL https://raw.githubusercontent.com/TsukiNi22/libutils/main/setup.sh | $SUDO bash
        if command -v dnf > /dev/null; then $SUDO dnf install -y libutils; else $SUDO apt-get install -y libutils; fi
    else
        local tmp
        tmp="$(mktemp -d)"
        git clone --depth 1 https://github.com/TsukiNi22/libutils.git "$tmp/libutils"
        cmake -S "$tmp/libutils" -B "$tmp/libutils/build" -DCMAKE_BUILD_TYPE=Optimized
        cmake --build "$tmp/libutils/build" --parallel
        $SUDO cmake --build "$tmp/libutils/build" --target install
        rm -rf "$tmp"
    fi
}

if [[ "$DEPS" -eq 1 ]]; then
    say "installing the build dependencies"
    if command -v dnf > /dev/null; then
        $SUDO dnf install -y cmake clang ccache python3 qt6-qtbase-devel openssl-devel gtest-devel libXtst-devel libei git curl
    elif command -v apt-get > /dev/null; then
        $SUDO apt-get update
        $SUDO apt-get install -y cmake clang ccache python3 qt6-base-dev libssl-dev libgtest-dev libxtst-dev libei1 git curl
    elif command -v pacman > /dev/null; then
        $SUDO pacman -S --needed --noconfirm cmake clang ccache python qt6-base openssl gtest libxtst libei git curl
    elif command -v brew > /dev/null; then
        brew install cmake llvm ccache python qt openssl@3 googletest git
    else
        die "unknown package manager: install cmake, clang, Qt 6 (Core Gui Widgets Network DBus), OpenSSL >= 3.2 yourself"
    fi
    install_libutils
fi

# ===== Build =====
command -v cmake > /dev/null || die "cmake missing (run with --deps)"
say "building xmouse (Optimized)"
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Optimized > /dev/null
cmake --build "$BUILD" --parallel
[[ -x "$ROOT/xmouse" ]] || die "build failed"

# ===== Install =====
mkdir -p "$PREFIX/bin" "$HOME/.local/share/applications" "$HOME/.local/share/icons/hicolor/scalable/apps"
install -m 0755 "$ROOT/xmouse" "$PREFIX/bin/xmouse"
install -m 0644 "$ROOT/docs/logo.svg" "$HOME/.local/share/icons/hicolor/scalable/apps/xmouse.svg"
cat > "$HOME/.local/share/applications/xmouse.desktop" << DESKTOP
[Desktop Entry]
Type=Application
Name=xmouse
Comment=One cursor, keyboard and clipboard for every device of your desk
Exec=$PREFIX/bin/xmouse --gui
Icon=xmouse
Categories=Utility;Network;
Terminal=false
DESKTOP
if [[ "$AUTOSTART" -eq 1 ]]; then
    mkdir -p "$HOME/.config/autostart"
    cp "$HOME/.local/share/applications/xmouse.desktop" "$HOME/.config/autostart/xmouse.desktop"
    say "xmouse starts at login (set startMinimized in the settings to start in the tray)"
fi

# ===== GNOME Shell extension (edges without the portal dialog) =====
if [[ "$GNOME" -eq 1 ]]; then
    mkdir -p "$EXTENSION_DIR"
    cp "$ROOT/gnome-extension/$EXTENSION/"* "$EXTENSION_DIR/"
    # A new extension is only seen by GNOME Shell after a new login (Wayland): enabled now in the settings if needed
    if ! gnome-extensions enable "$EXTENSION" 2> /dev/null; then
        current="$(gsettings get org.gnome.shell enabled-extensions)"
        case "$current" in
            *"'$EXTENSION'"*) ;;
            "@as []") gsettings set org.gnome.shell enabled-extensions "['$EXTENSION']" ;;
            *) gsettings set org.gnome.shell enabled-extensions "${current%]}, '$EXTENSION']" ;;
        esac
        say "GNOME extension installed: active after your next login (then no more input capture dialog)"
    else
        say "GNOME extension installed and enabled: restart xmouse"
    fi
fi

# ===== uinput (clicks / keys of the other devices on Wayland) =====
if [[ "$UINPUT" -eq 1 ]]; then
    # uaccess: the user logged in at the screen gets an ACL on /dev/uinput (no group change: works next to the xdesk rule)
    say "giving the user at the screen access to /dev/uinput (sudo)"
    echo 'KERNEL=="uinput", SUBSYSTEM=="misc", TAG+="uaccess", OPTIONS+="static_node=uinput"' | $SUDO tee /etc/udev/rules.d/61-xmouse-uinput.rules > /dev/null
    echo uinput | $SUDO tee /etc/modules-load.d/xmouse-uinput.conf > /dev/null
    $SUDO modprobe uinput || true
    $SUDO udevadm control --reload-rules && $SUDO udevadm trigger /dev/uinput || true
    say "the other devices can click and type here now (Wayland too)"
fi

case ":$PATH:" in
    *":$PREFIX/bin:"*) ;;
    *) say "add $PREFIX/bin to your PATH" ;;
esac
say "installed: $("$PREFIX/bin/xmouse" --version)"
say "start: xmouse (create a network, or join one with the code of a member)"
