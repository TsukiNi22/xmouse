#!/usr/bin/env bash
#
# Edition:
##  22/11/2026 by Tsukini
#
# File Name:
##  bundle_windows.sh
#
# File Description:
##  Cross compile xmouse.exe (MinGW-w64), pack it with every dll it needs (stripped): dist/xmouse-windows-x64.zip,
##  and with --installer the setup: dist/xmouse-windows-x64-setup.exe (NSIS, LZMA)
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SYSROOT="${MINGW_SYSROOT:-/usr/x86_64-w64-mingw32/sys-root/mingw}"
BUILD="$ROOT/build-windows"
DIST="$ROOT/dist/xmouse-windows-x64"
ZIP="$ROOT/dist/xmouse-windows-x64.zip"
SETUP="$ROOT/dist/xmouse-windows-x64-setup.exe"
OBJDUMP="${OBJDUMP:-x86_64-w64-mingw32-objdump}"
STRIP="${STRIP:-x86_64-w64-mingw32-strip}"
MAKENSIS="${MAKENSIS:-makensis}" # (mingw32-nsis)
BUILD_EXE=1
INSTALLER=0

usage() {
    echo "Usage: scripts/bundle_windows.sh [--no-build] [--installer]"
    echo "  --no-build   pack the xmouse.exe already built at the root (the CI builds it with the packages)"
    echo "  --installer  also build the setup (NSIS: mingw32-nsis, MAKENSIS=<command> for another makensis)"
    exit 1
}

for arg in "$@"; do
    case "$arg" in
        --no-build) BUILD_EXE=0 ;;
        --installer) INSTALLER=1 ;;
        -h|--help) usage ;;
        *) echo "Error: unknown argument '$arg'" >&2; usage ;;
    esac
done

die() { echo "bundle_windows: $*" >&2; exit 1; }
[[ -d "$SYSROOT/bin" ]] || die "MinGW sysroot not found ($SYSROOT)"
[[ -f "$SYSROOT/lib/qt6/plugins/platforms/qwindows.dll" ]] || die "Qt 6 for MinGW missing: sudo dnf install mingw64-gcc-c++ mingw64-qt6-qtbase mingw64-openssl (+ mingw64-libutils from the libutils mirror)"
command -v "$OBJDUMP" > /dev/null || die "$OBJDUMP missing (mingw64-binutils)"
command -v zip > /dev/null || die "zip missing"
command -v "$STRIP" > /dev/null || die "$STRIP missing (mingw64-binutils)"
if [[ "$INSTALLER" -eq 1 ]]; then
    command -v "${MAKENSIS%% *}" > /dev/null || die "makensis missing (sudo dnf install mingw32-nsis)"
fi

# Build
if [[ "$BUILD_EXE" -eq 1 ]]; then
    cmake -S "$ROOT" -B "$BUILD" -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/toolchain/mingw-w64.cmake" -DCMAKE_BUILD_TYPE=Optimized
    cmake --build "$BUILD" --parallel
fi
[[ -f "$ROOT/xmouse.exe" ]] || die "xmouse.exe not built"

# The exe, the Qt plugins it loads at run time, then every dll they import (recursively, from the sysroot)
rm -rf "$DIST"
mkdir -p "$DIST/platforms" "$DIST/tls" "$DIST/styles" "$DIST/imageformats"
cp "$ROOT/xmouse.exe" "$DIST/"
QT_PLUGINS="$SYSROOT/lib/qt6/plugins"
cp "$QT_PLUGINS/platforms/qwindows.dll" "$DIST/platforms/"
cp "$QT_PLUGINS/tls/qopensslbackend.dll" "$DIST/tls/" 2> /dev/null || cp "$QT_PLUGINS"/tls/*openssl*.dll "$DIST/tls/"
cp "$QT_PLUGINS"/styles/*.dll "$DIST/styles/" 2> /dev/null || true
cp "$QT_PLUGINS/imageformats/qsvg.dll" "$DIST/imageformats/" 2> /dev/null || true
declare -A seen=()
queue=("$DIST/xmouse.exe" "$DIST"/platforms/*.dll "$DIST"/tls/*.dll)
while [[ ${#queue[@]} -gt 0 ]]; do
    file="${queue[0]}"
    queue=("${queue[@]:1}")
    for dll in $("$OBJDUMP" -p "$file" | sed -n 's/^\s*DLL Name: //p'); do
        [[ -n "${seen[$dll]:-}" ]] && continue
        seen[$dll]=1
        if [[ -f "$SYSROOT/bin/$dll" ]]; then
            cp "$SYSROOT/bin/$dll" "$DIST/"
            queue+=("$DIST/$dll")
        fi
    done
done
# OpenSSL is loaded at run time by the tls plugin (not imported)
cp "$SYSROOT"/bin/libssl-*.dll "$SYSROOT"/bin/libcrypto-*.dll "$DIST/" 2> /dev/null || true
cp "$ROOT/scripts/install.ps1" "$ROOT/LICENSE" "$DIST/"
printf '[Paths]\nPlugins = .\n' > "$DIST/qt.conf"

# No debug symbols in what is shipped (libstdc++ 28 MB -> 2 MB, the whole folder 135 MB -> 80 MB)
find "$DIST" \( -name "*.dll" -o -name "*.exe" \) -exec "$STRIP" --strip-unneeded {} +

(cd "$ROOT/dist" && rm -f "$ZIP" && zip -qr "$ZIP" xmouse-windows-x64)
echo "bundle_windows: $ZIP ($(du -h "$ZIP" | cut -f1))"

# Installer: the same files, LZMA solid (about 22 MB)
if [[ "$INSTALLER" -eq 1 ]]; then
    VERSION="$(sed -n 's/^project(${TARGET} VERSION \([0-9.]*\) .*/\1/p' "$ROOT/CMakeLists.txt")"
    [[ -n "$VERSION" ]] || die "version not found in CMakeLists.txt"
    # shellcheck disable=SC2086 # MAKENSIS may be a command with its arguments (a wrapper)
    $MAKENSIS -V2 -DVERSION="$VERSION" -DSOURCE="$DIST" -DOUTFILE="$SETUP" "$ROOT/packaging/windows/xmouse.nsi"
    echo "bundle_windows: $SETUP ($(du -h "$SETUP" | cut -f1))"
fi
