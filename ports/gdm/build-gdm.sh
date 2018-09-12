#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/gdm"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}

mkdir -p "$BUILD_DIR" "$SYSROOT/bin"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)

TARGET=${TARGET:-i686-openhobbyos}
CC="$ROOT/toolchain/bin/$TARGET-gcc"

INCLUDES="-I$ROOT/user/lib -I$SYSROOT/include"
CFLAGS="--sysroot=$SYSROOT -O2 -ffreestanding -fno-pic -fno-pie $INCLUDES"
LIBS="-L$SYSROOT/lib -lopenhobbyosgloss -lc -lm -lgcc"

echo "[gdm] Compiling..."
$CC $CFLAGS -c "$ROOT/user/gdm.c" -o "$BUILD_DIR/gdm.o"

echo "[gdm] Linking..."
$CC $CFLAGS "$BUILD_DIR/gdm.o" $LIBS \
    -static -nostartfiles -Wl,-T,"$ROOT/user.ld" \
    -o "$BUILD_DIR/gdm"

cp "$BUILD_DIR/gdm" "$SYSROOT/bin/gdm"
$TARGET-strip "$BUILD_DIR/gdm" 2>/dev/null || true

echo "[gdm] Built: $SYSROOT/bin/gdm"
touch "$BUILD_DIR/.built"
