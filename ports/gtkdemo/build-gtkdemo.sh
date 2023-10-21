#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/gtkdemo"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}

mkdir -p "$BUILD_DIR" "$SYSROOT/bin"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)

TARGET=${TARGET:-i686-openhobbyos}
export PATH="$ROOT/toolchain/bin:$PATH"

CC="$ROOT/toolchain/bin/$TARGET-gcc"
OBJCOPY="$ROOT/toolchain/bin/$TARGET-objcopy"
PKG_CONFIG="$ROOT/toolchain/bin/$TARGET-pkg-config"

export PKG_CONFIG_LIBDIR="$SYSROOT/lib/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
export PKG_CONFIG_PATH=""

GTK_CFLAGS=$($PKG_CONFIG --cflags gtk4)
GTK_LIBS=$($PKG_CONFIG --libs gtk4)

CFLAGS="--sysroot=$SYSROOT -O2 -ffreestanding -fno-pic -fno-pie -D_GNU_SOURCE $GTK_CFLAGS"
LDFLAGS="--sysroot=$SYSROOT -static -nostartfiles -Wl,-T,$ROOT/user.ld"
LDFLAGS="$LDFLAGS -Wl,--start-group $GTK_LIBS -lm -lc -lgcc -lglibstubs -Wl,--end-group"
LDFLAGS="$LDFLAGS -lopenhobbyosgloss -Wl,--allow-multiple-definition"

echo "[gtkdemo] Compiling..."
$CC $CFLAGS -c "$ROOT/user/gtkdemo/gtkdemo.c" -o "$BUILD_DIR/gtkdemo.o"
echo "[gtkdemo] Compiling GL stubs..."
$CC $CFLAGS -c "$ROOT/ports/gtkdemo/gl_stubs.c" -o "$BUILD_DIR/gl_stubs.o"

echo "[gtkdemo] Linking..."
$CC $CFLAGS "$BUILD_DIR/gtkdemo.o" "$BUILD_DIR/gl_stubs.o" \
    $LDFLAGS \
    -o "$BUILD_DIR/gtkdemo"

echo "[gtkdemo] Installing..."
cp "$BUILD_DIR/gtkdemo" "$SYSROOT/bin/gtkdemo"
$OBJCOPY --only-keep-debug "$BUILD_DIR/gtkdemo" "$BUILD_DIR/gtkdemo.debug" 2>/dev/null || true
$TARGET-strip "$BUILD_DIR/gtkdemo" 2>/dev/null || true

echo "[gtkdemo] Built: $SYSROOT/bin/gtkdemo"
touch "$BUILD_DIR/.built"
