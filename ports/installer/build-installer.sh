#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/installer"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}

mkdir -p "$BUILD_DIR" "$SYSROOT/bin"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)

TARGET=${TARGET:-i686-openhobbyos}
CXX="$ROOT/toolchain/bin/$TARGET-g++"
OBJCOPY="$ROOT/toolchain/bin/$TARGET-objcopy"

INCLUDES="-I$ROOT/user/lib -I$ROOT/include -I$ROOT/user/lib/ohui -I$SYSROOT/include"
CXXFLAGS="--sysroot=$SYSROOT -O2 -ffreestanding -fno-pic -fno-pie -fno-exceptions -fno-rtti -std=c++11 $INCLUDES"
LIBS="-L$SYSROOT/lib -lgcc"

echo "[installer] Compiling..."
$CXX $CXXFLAGS -c "$ROOT/user/installer/installer.cpp" -o "$BUILD_DIR/installer.o"

echo "[installer] Linking..."
$CXX $CXXFLAGS \
    "$BUILD_DIR/installer.o" \
    "$ROOT/build/user/lib/start.o" \
    "$ROOT/build/user/lib/syscall.o" \
    "$ROOT/build/user/lib/runtime.o" \
    $LIBS \
    -static -nostartfiles -Wl,-T,"$ROOT/user.ld" \
    -o "$BUILD_DIR/installer"

cp "$BUILD_DIR/installer" "$SYSROOT/bin/installer"
$OBJCOPY --only-keep-debug "$BUILD_DIR/installer" "$BUILD_DIR/installer.debug" 2>/dev/null || true
$TARGET-strip "$BUILD_DIR/installer" 2>/dev/null || true

echo "[installer] Built: $SYSROOT/bin/installer"
touch "$BUILD_DIR/.built"
