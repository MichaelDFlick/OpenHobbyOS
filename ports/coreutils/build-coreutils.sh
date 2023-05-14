#!/usr/bin/env bash
# Build script for GNU coreutils.
# Prerequisites:
#   - git submodule update --init user/coreutils  (or the submodule already cloned)
#   - gnulib submodule inside user/coreutils/gnulib/
#   - Patches config.sub to recognize i686-openhobbyos
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/coreutils"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
COREDIR="$ROOT/user/coreutils"
TARGET=${TARGET:-i686-openhobbyos}
JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)}

mkdir -p "$BUILD_DIR" "$SYSROOT"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)

export PATH="$ROOT/toolchain/bin:$PATH"
export CC="${TARGET}-gcc"
export CFLAGS="--sysroot=$SYSROOT -O2 -ffreestanding -fno-pic -fno-pie"
export LDFLAGS="--sysroot=$SYSROOT -static -nostartfiles -lopenhobbyosgloss"

# Bootstrap coreutils if configure is missing
if [ ! -f "$COREDIR/configure" ]; then
    pushd "$COREDIR" >/dev/null
    # Patch config.sub to recognize our OS
    if ! grep -q "openhobbyos" build-aux/config.sub 2>/dev/null; then
        sed -i '/^[[:space:]]*| ohos\*/a\	| openhobbyos* \\' build-aux/config.sub
    fi
    if ! grep -q "openhobbyos" gnulib/build-aux/config.sub 2>/dev/null; then
        sed -i '/^[[:space:]]*| ohos\*/a\	| openhobbyos* \\' gnulib/build-aux/config.sub
    fi
    # Run bootstrap with local gnulib (no network needed)
    ./bootstrap --no-git --gnulib-srcdir=gnulib 2>&1 | tail -5
    popd >/dev/null
fi

cd "$BUILD_DIR"

if [ ! -f Makefile ]; then
    "$COREDIR/configure" \
        --host="$TARGET" \
        --prefix=/ \
        --disable-nls \
        --disable-rpath \
        --disable-libcap \
        --disable-gcc-warnings \
        --enable-no-install-program=arch,chcon,chgrp,chroot,comm,cpio,csplit,df,dir,\
dircolors,du,env,expand,expr,factor,find,fmt,fold,groups,hostid,id,install,join,kill,\
md5sum,mkfifo,mknod,nl,nohup,numfmt,paste,pinky,pr,printenv,ptx,runcon,sha1sum,\
sha224sum,sha256sum,sha384sum,sha512sum,shred,shuf,split,stdbuf,stty,sum,timeout,\
tsort,tty,unexpand,uptime,users,vdir
fi

make -j"$JOBS"
make install DESTDIR="$SYSROOT"
