#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/harfbuzz"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-openhobbyos}
CROSS_FILE="$BUILD_DIR/openhobbyos.cross"
HB_SRC="$ROOT/user/lib/harfbuzz"

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

echo "[harfbuzz] Generating cross-compilation file..."
sed \
    -e "s|@TARGET@|$TARGET|g" \
    -e "s|@SYSROOT@|$SYSROOT|g" \
    "$ROOT/ports/meson/openhobbyos.cross.in" > "$CROSS_FILE"

# HarfBuzz needs a C++ compiler
sed -i "/^c = /a cpp = '$TARGET-g++'" "$CROSS_FILE"

HB_OVERRIDES="$ROOT/ports/harfbuzz/overrides"
# Add override include path to c_args and cpp_args
sed -i "s|c_args = .*|c_args = ['--sysroot=$SYSROOT', '-O2', '-ffreestanding', '-fno-pic', '-fno-pie', '-I$HB_OVERRIDES']|" "$CROSS_FILE"
# cpp_args is separate in newer meson; don't use -ffreestanding for C++
# because GCC's libstdc++ headers (e.g. <cmath>) refuse to work in freestanding
sed -i "/^c_link_args = /i cpp_args = ['--sysroot=$SYSROOT', '-O2', '-fno-pic', '-fno-pie', '-I$HB_OVERRIDES']" "$CROSS_FILE"

echo "[harfbuzz] Configuring..."
"$MESON" setup \
    "$BUILD_DIR/build" \
    "$HB_SRC" \
    --cross-file "$CROSS_FILE" \
    --prefix / \
    --bindir bin \
    --libdir lib \
    --includedir include \
    --datadir share \
    -Ddefault_library=static \
    -Dfreetype=enabled \
    -Dglib=enabled \
    -Dcairo=disabled \
    -Dicu=disabled \
    -Dgraphite2=disabled \
    -Dgobject=disabled \
    -Dintrospection=disabled \
    -Ddocs=disabled \
    -Dtests=disabled \
    -Dutilities=disabled \
    -Dpng=disabled \
    -Dzlib=disabled \
    -Dsubset=disabled \
    -Draster=disabled \
    -Dvector=disabled \
    -Dgpu=disabled \
    -Dbenchmark=disabled \
    -Dexperimental_api=false \
    --wrap-mode=nofallback \
    --buildtype release

echo "[harfbuzz] Building..."
"$MESON" compile -C "$BUILD_DIR/build"

echo "[harfbuzz] Installing..."
"$MESON" install -C "$BUILD_DIR/build" --destdir "$SYSROOT" --no-rebuild

# Generate harfbuzz-subset stub pc (GTK needs this but we build with -Dsubset=disabled)
cat > "$SYSROOT/lib/pkgconfig/harfbuzz-subset.pc" << EOF
prefix=/
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: harfbuzz-subset
Description: harfbuzz subset library
Version: 14.2.1
Requires: harfbuzz
Libs: -L\${libdir} -lharfbuzz
Cflags: -I\${includedir}/harfbuzz
EOF

echo "[harfbuzz] Built and installed successfully"
