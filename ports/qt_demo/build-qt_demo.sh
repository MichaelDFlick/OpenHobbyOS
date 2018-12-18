#!/usr/bin/env bash
# Build the Qt demo program for OpenHobbyOS.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
SYSROOT=${1:-"$ROOT/build/ports/sysroot"}
QT_SRC="$ROOT/user/lib/graphics32/qtbase"
QT_BUILD="$ROOT/build/ports/qt"
QT_HOST_INC="$QT_BUILD/host/include"
QT_CROSS_LIB="$QT_BUILD/cross/lib"
INSTALL_DIR="$SYSROOT/bin"

export PATH="$ROOT/toolchain/bin:$PATH"
export OPENHOBBYOS_SYSROOT="$SYSROOT"

mkdir -p "$INSTALL_DIR"

UNWIND_STUBS="$ROOT/build/user/qt_unwind_stubs.o"
i686-openhobbyos-gcc -c -O2 "$ROOT/user/qt_demo/unwind_stubs.c" -o "$UNWIND_STUBS"

i686-openhobbyos-g++ \
    -std=gnu++17 \
    -O2 \
    -fPIC \
    -fno-rtti \
    -I"$QT_HOST_INC" \
    -I"$QT_SRC" \
    -o "$INSTALL_DIR/qt_demo" \
    "$ROOT/user/qt_demo/qt_demo.cpp" \
    "$UNWIND_STUBS" \
    -L"$QT_CROSS_LIB" \
    -L"$SYSROOT/lib" \
    -L"$SYSROOT/i686-elf/lib" \
    -static \
    -lQt6Widgets \
    -lQt6Gui \
    -lQt6Core \
    -lQt6BundledFreetype \
    -lQt6BundledPcre2 \
    -lQt6BundledLibpng \
    -lQt6BundledHarfbuzz \
    -lpixman-1 \
    -lxnx \
    -lopenhobbyosgloss \
    -lz \
    -lm \
    -lgcc

i686-openhobbyos-strip "$INSTALL_DIR/qt_demo"
echo "[qt_demo] built $INSTALL_DIR/qt_demo"
