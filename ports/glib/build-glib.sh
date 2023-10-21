#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/glib"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-openhobbyos}
CROSS_FILE="$BUILD_DIR/openhobbyos.cross"
GLIB_SRC="$ROOT/user/lib/glib"

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

GLIB_OVERRIDES="$ROOT/ports/glib/overrides"

echo "[glib] Building stub libraries..."
STUB_DIR="$BUILD_DIR/stubs"
mkdir -p "$STUB_DIR" "$SYSROOT/i686-elf/lib"
$TARGET-gcc --sysroot="$SYSROOT" -I"$GLIB_OVERRIDES" -c "$ROOT/ports/glib/resolv_stub.c" -o "$STUB_DIR/resolv_stub.o"
$TARGET-gcc --sysroot="$SYSROOT" -I"$GLIB_OVERRIDES" -c "$ROOT/ports/glib/glib_stubs.c" -o "$STUB_DIR/glib_stubs.o"
$TARGET-ar rcs "$STUB_DIR/libresolv.a" "$STUB_DIR/resolv_stub.o"
$TARGET-ar rcs "$STUB_DIR/libglibstubs.a" "$STUB_DIR/glib_stubs.o"
cp "$STUB_DIR/libresolv.a" "$SYSROOT/i686-elf/lib/libresolv.a"
cp "$STUB_DIR/libglibstubs.a" "$SYSROOT/i686-elf/lib/libglibstubs.a"

echo "[glib] Generating cross-compilation file..."
sed \
    -e "s|@TARGET@|$TARGET|g" \
    -e "s|@SYSROOT@|$SYSROOT|g" \
    "$ROOT/ports/meson/openhobbyos.cross.in" > "$CROSS_FILE"

sed -i "s|c_args = .*|c_args = ['--sysroot=$SYSROOT', '-O2', '-ffreestanding', '-fno-pic', '-fno-pie', '-I$GLIB_OVERRIDES', '-D_POSIX_HOST_NAME_MAX=255', '-DSSIZE_MAX=2147483647', '-DNI_MAXHOST=1025', '-DHAVE_NETLINK=0']|" "$CROSS_FILE"
sed -i "s|c_link_args = .*|c_link_args = ['--sysroot=$SYSROOT', '-static', '-nostartfiles', '-Wl,-T,$SYSROOT/lib/user.ld', '-lopenhobbyosgloss', '-lglibstubs', '-lresolv']|" "$CROSS_FILE"

echo "[glib] Configuring..."
"$MESON" setup \
    "$BUILD_DIR/build" \
    "$GLIB_SRC" \
    --cross-file "$CROSS_FILE" \
    --prefix / \
    --bindir bin \
    --libdir lib \
    --includedir include \
    --datadir share \
    -Ddefault_library=static \
    -Dtests=false \
    -Dinstalled_tests=false \
    -Ddocumentation=false \
    -Dnls=disabled \
    -Dintrospection=disabled \
    -Dselinux=disabled \
    -Dxattr=false \
    -Dlibmount=disabled \
    -Ddtrace=disabled \
    -Dsystemtap=disabled \
    -Dsysprof=disabled \
    -Dman-pages=disabled \
    -Dlibelf=disabled \
    -Dglib_debug=disabled \
    -Dglib_assert=false \
    -Dglib_checks=false \
    -Dforce_posix_threads=true \
    --wrap-mode=nodownload \
    --force-fallback-for=intl \
    --buildtype release

echo "[glib] Building..."
"$MESON" compile -C "$BUILD_DIR/build"

echo "[glib] Installing..."
"$MESON" install -C "$BUILD_DIR/build" --destdir "$SYSROOT" --no-rebuild

echo "[glib] Built and installed successfully"
