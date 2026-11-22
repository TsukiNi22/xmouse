#!/usr/bin/env bash
#
# Edition:
##  22/11/2026 by Tsukini
#
# File Name:
##  bundle_linux.sh
#
# File Description:
##  Pack the Linux build of xmouse for the distributions without its package repository: dist/xmouse-linux-x86_64.tar.gz
##  (bin/xmouse stripped, menu entry, icon, GNOME extension), unpacked into ~/.local (or /usr/local)
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build-release"
DIST="$ROOT/dist/xmouse-linux-x86_64"
TARBALL="$ROOT/dist/xmouse-linux-x86_64.tar.gz"
EXTENSION="xmouse@tsukini22.github.io"
BUILD_EXE=1

usage() {
    echo "Usage: scripts/bundle_linux.sh [--no-build]"
    echo "  --no-build   pack the xmouse already built at the root (the CI builds it with the packages)"
    exit 1
}

for arg in "$@"; do
    case "$arg" in
        --no-build) BUILD_EXE=0 ;;
        -h|--help) usage ;;
        *) echo "Error: unknown argument '$arg'" >&2; usage ;;
    esac
done

die() { echo "bundle_linux: $*" >&2; exit 1; }

# Build
if [[ "$BUILD_EXE" -eq 1 ]]; then
    cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Optimized
    cmake --build "$BUILD" --parallel
fi
[[ -x "$ROOT/xmouse" ]] || die "xmouse not built"
# The Optimized build only: the tests and the Debug / Asan builds write the same ./xmouse (DWARF or the sanitizer in it)
SECTIONS="$(readelf -S "$ROOT/xmouse")"
SYMBOLS="$(nm -D "$ROOT/xmouse" 2> /dev/null || true)"
if [[ "$SECTIONS" == *".debug_info"* || "$SYMBOLS" == *"__asan_init"* ]]; then
    die "./xmouse is a Debug or Asan build: rebuild the Optimized one (cmake -S . -B build -DCMAKE_BUILD_TYPE=Optimized, rm xmouse, cmake --build build)"
fi

# The layout of a prefix: tar -xzf xmouse-linux-x86_64.tar.gz -C ~/.local --strip-components=1
rm -rf "$DIST"
mkdir -p "$DIST/bin" "$DIST/share/applications" "$DIST/share/icons/hicolor/scalable/apps" "$DIST/share/gnome-shell/extensions/$EXTENSION" "$DIST/share/licenses/xmouse"
install -m 0755 "$ROOT/xmouse" "$DIST/bin/xmouse"
strip --strip-unneeded "$DIST/bin/xmouse"
install -m 0644 "$ROOT/docs/logo.svg" "$DIST/share/icons/hicolor/scalable/apps/xmouse.svg"
install -m 0644 "$ROOT/gnome-extension/$EXTENSION/"* "$DIST/share/gnome-shell/extensions/$EXTENSION/"
install -m 0644 "$ROOT/LICENSE" "$DIST/share/licenses/xmouse/LICENSE"
cat > "$DIST/share/applications/xmouse.desktop" << DESKTOP
[Desktop Entry]
Type=Application
Name=xmouse
Comment=One cursor, keyboard and clipboard for every device of your desk
Exec=xmouse --gui
Icon=xmouse
Categories=Utility;Network;
Terminal=false
DESKTOP
cat > "$DIST/README.txt" << README
xmouse $("$ROOT/xmouse" --version | sed -n 's/^xmouse \([0-9.]*\).*/\1/p') for Linux x86_64

Install (your user, no root):
    tar -xzf xmouse-linux-x86_64.tar.gz -C ~/.local --strip-components=1
    (~/.local/bin in the PATH; GNOME: gnome-extensions enable $EXTENSION, then a new login)
Needs: Qt 6 >= 6.4 (Core Gui Widgets Network DBus), OpenSSL >= 3.2, libei (Wayland edges), a glibc as recent as the
build system (Fedora 44). Otherwise: the packages (setup.sh of the repository) or the sources (scripts/install.sh).
Wayland: the other devices click / type here once /dev/uinput is writable (scripts/install.sh --uinput of the sources).
README

(cd "$ROOT/dist" && rm -f "$TARBALL" && tar -czf "$TARBALL" xmouse-linux-x86_64)
echo "bundle_linux: $TARBALL ($(du -h "$TARBALL" | cut -f1))"
