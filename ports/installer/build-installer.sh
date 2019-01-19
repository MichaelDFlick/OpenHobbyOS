#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/installer"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}

mkdir -p "$BUILD_DIR" "$SYSROOT/bin"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)

TARGET=${TARGET:-i686-openhobbyos}
CC="$ROOT/toolchain/bin/$TARGET-gcc"
OBJCOPY="$ROOT/toolchain/bin/$TARGET-objcopy"

export PKG_CONFIG_LIBDIR="$SYSROOT/lib/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
export PKG_CONFIG_PATH=""

INCLUDES="-I$ROOT/user/lib -I$ROOT/user/lib/ohui -I$SYSROOT/include -I$SYSROOT/include/cairo -I$SYSROOT/include/freetype2 -I$SYSROOT/include/pixman-1 -I$SYSROOT/include/xnx"
CFLAGS="--sysroot=$SYSROOT -O2 -ffreestanding -fno-pic -fno-pie $INCLUDES"
LIBS="-L$SYSROOT/lib -lohui -lxnx -lcairo -lpixman-1 -lfreetype -lpng16 -lz -lm -lgcc"

echo "[installer] Compiling..."
$CC $CFLAGS -c "$ROOT/user/installer/installer.c" -o "$BUILD_DIR/installer.o"

echo "[installer] Linking..."
$CC $CFLAGS "$BUILD_DIR/installer.o" $LIBS \
    -static -nostartfiles -Wl,-T,"$ROOT/user.ld" \
    -o "$BUILD_DIR/installer"

cp "$BUILD_DIR/installer" "$SYSROOT/bin/installer"
$OBJCOPY --only-keep-debug "$BUILD_DIR/installer" "$BUILD_DIR/installer.debug" 2>/dev/null || true
$TARGET-strip "$BUILD_DIR/installer" 2>/dev/null || true

echo "[installer] Built: $SYSROOT/bin/installer"
touch "$BUILD_DIR/.built"
