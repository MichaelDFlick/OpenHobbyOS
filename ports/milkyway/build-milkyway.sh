#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/milkyway"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}

mkdir -p "$BUILD_DIR" "$SYSROOT/bin"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)

TARGET=${TARGET:-i686-openhobbyos}
CXX="$ROOT/toolchain/bin/$TARGET-g++"
OBJCOPY="$ROOT/toolchain/bin/$TARGET-objcopy"

INCLUDES="-I$ROOT/user/lib -I$ROOT/include -I$SYSROOT/include"
CXXFLAGS="--sysroot=$SYSROOT -O2 -ffreestanding -fno-pic -fno-pie -fno-exceptions -fno-rtti -std=c++11 $INCLUDES"
LIBS="-L$SYSROOT/lib -lgcc"

echo "[milkyway] Compiling..."
$CXX $CXXFLAGS -c "$ROOT/user/milkyway/milkyway.cpp" -o "$BUILD_DIR/milkyway.o"

echo "[milkyway] Linking..."
$CXX $CXXFLAGS \
    "$BUILD_DIR/milkyway.o" \
    "$ROOT/build/user/lib/start.o" \
    "$ROOT/build/user/lib/syscall.o" \
    "$ROOT/build/user/lib/runtime.o" \
    $LIBS \
    -static -nostartfiles -Wl,-T,"$ROOT/user.ld" \
    -o "$BUILD_DIR/milkyway"

cp "$BUILD_DIR/milkyway" "$SYSROOT/bin/milkyway"
$OBJCOPY --only-keep-debug "$BUILD_DIR/milkyway" "$BUILD_DIR/milkyway.debug" 2>/dev/null || true
$TARGET-strip "$BUILD_DIR/milkyway" 2>/dev/null || true

echo "[milkyway] Built: $SYSROOT/bin/milkyway"
touch "$BUILD_DIR/.built"
