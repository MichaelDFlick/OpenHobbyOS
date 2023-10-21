#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/graphene"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-openhobbyos}
GRAPHENE_SRC="$ROOT/user/lib/graphene"

mkdir -p "$BUILD_DIR" "$SYSROOT"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)
CROSS_FILE="$BUILD_DIR/openhobbyos.cross"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
MESON="$("$ROOT/tools/ensure_meson.sh")"
export PATH="$ROOT/toolchain/bin:$PATH"
export PKG_CONFIG_ALLOW_SYSTEM_CFLAGS=1
export PKG_CONFIG_ALLOW_SYSTEM_LIBS=1
export PKG_CONFIG_PATH="$SYSROOT/lib/pkgconfig:$SYSROOT/share/pkgconfig"

echo "[graphene] Generating cross-compilation file..."
sed \
    -e "s|@TARGET@|$TARGET|g" \
    -e "s|@SYSROOT@|$SYSROOT|g" \
    "$ROOT/ports/meson/openhobbyos.cross.in" > "$CROSS_FILE"

echo "[graphene] Configuring..."
"$MESON" setup \
    "$BUILD_DIR/build" \
    "$GRAPHENE_SRC" \
    --cross-file "$CROSS_FILE" \
    --prefix / \
    --bindir bin \
    --libdir lib \
    --includedir include \
    --datadir share \
    -Ddefault_library=static \
    -Dtests=false \
    -Dgobject_types=true \
    -Dintrospection=disabled \
    --buildtype release

echo "[graphene] Building..."
"$MESON" compile -C "$BUILD_DIR/build"

echo "[graphene] Installing..."
"$MESON" install -C "$BUILD_DIR/build" --destdir "$SYSROOT" --no-rebuild 2>/dev/null || true

for pc in graphene-1.0.pc graphene-gobject-1.0.pc; do
    src_pc="$BUILD_DIR/build/meson-private/$pc"
    if [ -f "$src_pc" ]; then
        cp "$src_pc" "$SYSROOT/lib/pkgconfig/$pc"
        echo "[graphene] Installed $pc"
    fi
done

echo "[graphene] Built and installed successfully"
