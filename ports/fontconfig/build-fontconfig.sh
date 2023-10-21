#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/fontconfig"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-openhobbyos}
CROSS_FILE="$BUILD_DIR/openhobbyos.cross"
FC_SRC="$ROOT/user/lib/pango/subprojects/fontconfig"

mkdir -p "$BUILD_DIR" "$SYSROOT"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
MESON="$("$ROOT/tools/ensure_meson.sh")"
export PATH="$ROOT/toolchain/bin:$PATH"
export OPENHOBBYOS_SYSROOT="$SYSROOT"
export PKG_CONFIG_ALLOW_SYSTEM_CFLAGS=1
export PKG_CONFIG_ALLOW_SYSTEM_LIBS=1
export PKG_CONFIG_PATH="$SYSROOT/lib/pkgconfig"

echo "[fontconfig] Generating cross-compilation file..."
sed \
    -e "s|@TARGET@|$TARGET|g" \
    -e "s|@SYSROOT@|$SYSROOT|g" \
    "$ROOT/ports/meson/openhobbyos.cross.in" > "$CROSS_FILE"

echo "[fontconfig] Configuring..."
"$MESON" setup \
    "$BUILD_DIR/build" \
    "$FC_SRC" \
    --cross-file "$CROSS_FILE" \
    --prefix / \
    --bindir bin \
    --libdir lib \
    --includedir include \
    --datadir share \
    -Ddefault_library=static \
    -Ddoc=disabled \
    -Dtests=disabled \
    -Dtools=disabled \
    -Dnls=disabled \
    -Dcache-build=disabled \
    --wrap-mode=nodownload \
    --buildtype release

echo "[fontconfig] Building..."
"$MESON" compile -C "$BUILD_DIR/build"

echo "[fontconfig] Installing..."
"$MESON" install -C "$BUILD_DIR/build" --destdir "$SYSROOT" --no-rebuild

echo "[fontconfig] Built and installed successfully"
