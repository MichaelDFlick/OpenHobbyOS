#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/gosh"}
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

INCLUDES="-I$ROOT/user/lib -I$SYSROOT/include -I$SYSROOT/include/cairo -I$SYSROOT/include/freetype2 -I$SYSROOT/include/pixman-1 -idirafter $ROOT/include"
CFLAGS="--sysroot=$SYSROOT -O2 -ffreestanding -fno-pic -fno-pie $INCLUDES"
LIBS="-L$SYSROOT/lib -lcairo -lpixman-1 -lfreetype -lz -lm -lgcc"

echo "[gosh] Compiling..."
$CC $CFLAGS -c "$ROOT/user/gosh.c" -o "$BUILD_DIR/gosh.o"
$CC $CFLAGS -c "$ROOT/user/lib/syscall.c" -o "$BUILD_DIR/syscall.o"
$CC $CFLAGS -c "$ROOT/user/lib/runtime.c" -o "$BUILD_DIR/runtime.o"

echo "[gosh] Linking..."
$CC $CFLAGS "$BUILD_DIR/gosh.o" "$BUILD_DIR/syscall.o" "$BUILD_DIR/runtime.o" $LIBS \
    -static -nostartfiles -Wl,-T,"$ROOT/user.ld" \
    -o "$BUILD_DIR/gosh"

cp "$BUILD_DIR/gosh" "$SYSROOT/bin/gosh"
$OBJCOPY --only-keep-debug "$BUILD_DIR/gosh" "$BUILD_DIR/gosh.debug" 2>/dev/null || true
$TARGET-strip "$BUILD_DIR/gosh" 2>/dev/null || true

echo "[gosh] Built: $SYSROOT/bin/gosh"
touch "$BUILD_DIR/.built"
