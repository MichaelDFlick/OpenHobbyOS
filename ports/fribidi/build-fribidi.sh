#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/fribidi"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-openhobbyos}
CROSS_FILE="$BUILD_DIR/openhobbyos.cross"
FRIBIDI_SRC="$ROOT/user/lib/pango/subprojects/fribidi"

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

echo "[fribidi] Generating cross-compilation file..."
sed \
    -e "s|@TARGET@|$TARGET|g" \
    -e "s|@SYSROOT@|$SYSROOT|g" \
    "$ROOT/ports/meson/openhobbyos.cross.in" > "$CROSS_FILE"

echo "[fribidi] Configuring..."
"$MESON" setup \
    "$BUILD_DIR/build" \
    "$FRIBIDI_SRC" \
    --cross-file "$CROSS_FILE" \
    --prefix / \
    --bindir bin \
    --libdir lib \
    --includedir include \
    -Ddefault_library=static \
    -Ddocs=false \
    -Dbin=false \
    -Dtests=false \
    --wrap-mode=nodownload \
    --buildtype release

echo "[fribidi] Building..."
"$MESON" compile -C "$BUILD_DIR/build"

echo "[fribidi] Installing..."
"$MESON" install -C "$BUILD_DIR/build" --destdir "$SYSROOT" --no-rebuild

echo "[fribidi] Built and installed successfully"
