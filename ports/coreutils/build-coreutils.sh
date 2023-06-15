#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/coreutils"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
COREDIR="$ROOT/user/coreutils"
TARGET=${TARGET:-i686-openhobbyos}
JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)}
mkdir -p "$BUILD_DIR" "$SYSROOT"
export PATH="$ROOT/toolchain/bin:$PATH"

if [ ! -f "$COREDIR/README" ]; then
    git -C "$ROOT" submodule update --init --recursive user/coreutils
fi
if [ ! -d "$COREDIR/gnulib/build-aux" ]; then
    (cd "$COREDIR" && git submodule update --init --recursive)
fi

# patch gnulib config.sub so i686-openhobbyos is recognized
CS="$COREDIR/gnulib/build-aux/config.sub"
if [ -f "$CS" ] && ! grep -q "openhobbyos" "$CS" 2>/dev/null; then
    sed -i 's/^\(	| ohos\* \\\)/\1\n	| openhobbyos* \\/' "$CS"
fi

if [ ! -f "$COREDIR/configure" ]; then
    pushd "$COREDIR" >/dev/null
    ./bootstrap --no-git --gnulib-srcdir=gnulib 2>&1 | tail -5
    popd >/dev/null
fi
# re-patch after bootstrap
if [ -f "$CS" ] && ! grep -q "openhobbyos" "$CS" 2>/dev/null; then
    sed -i 's/^\(	| ohos\* \\\)/\1\n	| openhobbyos* \\/' "$CS"
fi

export CC="${TARGET}-gcc"
export CFLAGS="--sysroot=$SYSROOT -O2 -ffreestanding -fno-pic -fno-pie"
export LDFLAGS="--sysroot=$SYSROOT -static -nostartfiles -lopenhobbyosgloss"

cd "$BUILD_DIR"
if [ ! -f Makefile ]; then
    "$COREDIR/configure" \
        --host="$TARGET" \
        --prefix=/ \
        --disable-nls \
        --disable-rpath \
        --disable-libcap \
        --disable-gcc-warnings \
        --enable-cross-guesses=risky
fi
make -j"$JOBS"
make install DESTDIR="$SYSROOT"
