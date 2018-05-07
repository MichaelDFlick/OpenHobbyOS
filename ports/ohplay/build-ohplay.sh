#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/ohplay"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
INSTALL_DIR=${3:-"$BUILD_DIR/install"}

TARGET=${TARGET:-i686-openhobbyos}
export PATH="$ROOT/toolchain/bin:$PATH"

CC=${CC:-"$TARGET-gcc"}

mkdir -p "$BUILD_DIR" "$SYSROOT" "$INSTALL_DIR"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)
INSTALL_DIR=$(cd "$INSTALL_DIR" && pwd)

rm -rf "$BUILD_DIR/obj" "$BUILD_DIR/bin"
mkdir -p "$BUILD_DIR/obj" "$BUILD_DIR/bin" "$INSTALL_DIR/bin" "$SYSROOT/bin"

if [ ! -f "$SYSROOT/lib/libavformat.a" ] || [ ! -f "$SYSROOT/lib/libxnx.a" ]; then
    echo "ERROR: FFmpeg and XNX must be built first"
    echo "  expected $SYSROOT/lib/libavformat.a and $SYSROOT/lib/libxnx.a"
    exit 1
fi

CFLAGS=(
    --sysroot="$SYSROOT"
    -O2
    -ffreestanding
    -fno-pic
    -fno-pie
    -D_GNU_SOURCE
    -D_DEFAULT_SOURCE
    -D_POSIX_C_SOURCE=200809L
    -D_XOPEN_SOURCE=700
    -I"$ROOT/user/lib"
    -I"$ROOT/user/lib/xnx"
    -I"$ROOT/user/xnx"
    -I"$SYSROOT/i686-elf/include"
    -I"$SYSROOT/include"
    -idirafter "$ROOT/include"
)
NEWLIB_LIBDIR="$SYSROOT/i686-elf/lib"

"$CC" "${CFLAGS[@]}" -c "$ROOT/user/lib/syscall.c" -o "$BUILD_DIR/obj/syscall.o"
"$CC" "${CFLAGS[@]}" -c "$ROOT/user/ohplay.c" -o "$BUILD_DIR/obj/ohplay.o"

"$CC" \
    --sysroot="$SYSROOT" \
    -static \
    -nostartfiles \
    -Wl,-T,"$SYSROOT/lib/user.ld" \
    -o "$BUILD_DIR/bin/ohplay" \
    "$BUILD_DIR/obj/ohplay.o" \
    -Wl,--start-group \
    -L"$SYSROOT/lib" \
    -L"$NEWLIB_LIBDIR" \
    "$BUILD_DIR/obj/syscall.o" \
    -lavformat \
    -lavcodec \
    -lavutil \
    -lswscale \
    -lswresample \
    -lxnx \
    -lm \
    -lz \
    -lc \
    -lgcc \
    -Wl,--end-group \
    -lopenhobbyosgloss

install -m 755 "$BUILD_DIR/bin/ohplay" "$INSTALL_DIR/bin/ohplay"
install -m 755 "$BUILD_DIR/bin/ohplay" "$SYSROOT/bin/ohplay"

echo "ohplay built successfully"
