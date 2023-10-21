#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/expat"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-openhobbyos}
EXPAT_SRC="$ROOT/user/lib/pango/subprojects/expat/expat"

mkdir -p "$BUILD_DIR" "$SYSROOT"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)
BUILD="$BUILD_DIR/build"

export PATH="$ROOT/toolchain/bin:$PATH"
export OPENHOBBYOS_SYSROOT="$SYSROOT"

CMAKE_TOOLCHAIN="$ROOT/ports/qt/openhobbyos-toolchain.cmake"

echo "[expat] Configuring..."
cmake -S "$EXPAT_SRC" -B "$BUILD" \
    -DCMAKE_TOOLCHAIN_FILE="$CMAKE_TOOLCHAIN" \
    -DCMAKE_INSTALL_PREFIX="$SYSROOT" \
    -DCMAKE_BUILD_TYPE=Release \
    -DEXPAT_BUILD_TESTS=OFF \
    -DEXPAT_BUILD_EXAMPLES=OFF \
    -DEXPAT_BUILD_TOOLS=OFF \
    -DEXPAT_SHARED_LIBS=OFF

echo "[expat] Building..."
cmake --build "$BUILD"

echo "[expat] Installing..."
cmake --install "$BUILD"

echo "[expat] Generating pkg-config file..."
mkdir -p "$SYSROOT/lib/pkgconfig"
cat > "$SYSROOT/lib/pkgconfig/expat.pc" << EOF
prefix=$SYSROOT
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: expat
Description: XML C parser
Version: 2.6.4
Libs: -L\${libdir} -lexpat
Cflags: -I\${includedir}
EOF

echo "[expat] Built and installed successfully"
