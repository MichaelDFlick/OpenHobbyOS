#!/usr/bin/env bash
# Build the OpenHobbyOS dynamic linker (ld-openhobbyos.so.1).
set -euo pipefail

ROOT=$(cd "$(dirname "$(readlink -f "$0")")/../../.." && pwd)
SYSROOT=${1:-"$ROOT/build/ports/sysroot"}
INSTALL_DIR="$SYSROOT/lib"

export PATH="$ROOT/toolchain/bin:$PATH"
export SYSROOT="$SYSROOT"

mkdir -p "$INSTALL_DIR"

i686-openhobbyos-gcc \
    -std=gnu11 \
    -O2 \
    -fno-builtin \
    -fno-stack-protector \
    -fomit-frame-pointer \
    -nostdlib \
    -ffreestanding \
    -Wl,-T,"$ROOT/user/lib/ld-openhobbyos/ld.ld" \
    -Wl,-e_start \
    -Wl,--build-id=none \
    -o "$INSTALL_DIR/ld-openhobbyos.so.1" \
    "$ROOT/user/lib/ld-openhobbyos/ld.c"

i686-openhobbyos-strip "$INSTALL_DIR/ld-openhobbyos.so.1"
echo "[ld-openhobbyos] built $INSTALL_DIR/ld-openhobbyos.so.1"
