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

# gnulib expects _GL_INLINE_HEADER_BEGIN/_GL_INLINE_HEADER_END to be defined in config.h
# but gl_EXTERN_INLINE's AH_VERBATIM doesn't always run in freestanding mode
# Patch config.hin so these macros get included in config.h (must be before configure)
CONFIG_HIN="$COREDIR/lib/config.hin"
if [ -f "$CONFIG_HIN" ] && ! grep -q "_GL_INLINE_HEADER_OPENHANDOS" "$CONFIG_HIN" 2>/dev/null; then
    # Append to config.hin (before the final #endif if present)
    cat >> "$CONFIG_HIN" << 'INLINEEOF'

/* OpenHobbyOS: Define inline macros that gnulib expects (normally from gl_EXTERN_INLINE) */
#define _GL_INLINE_HEADER_OPENHANDOS 1
#ifndef _GL_INLINE_HEADER_BEGIN
#define _GL_INLINE_HEADER_BEGIN
#endif
#ifndef _GL_INLINE_HEADER_END
#define _GL_INLINE_HEADER_END
#endif
#ifndef _GL_INLINE
#define _GL_INLINE inline
#endif
#ifndef _GL_EXTERN_INLINE
#define _GL_EXTERN_INLINE extern inline
#endif
INLINEEOF
fi

export CC="${TARGET}-gcc"
# GCC's i686-openhobbyos target is derived from i686-linux-gnu so it predefines
# __GLIBC__=2, causing gnulib to take glibc-specific code paths that break on
# newlib; override to 0 so the generic (portable) fallbacks are used.
export CFLAGS="--sysroot=$SYSROOT -O2 -ffreestanding -fno-pic -fno-pie -D__GLIBC__=0 -D__GLIBC_MINOR__=0 -D__OHOS__=1"
export LDFLAGS="--sysroot=$SYSROOT -static -nostartfiles -lopenhobbyosgloss"

# disable all runtime tests (cross-compile mode would work but some gnulib
# tests still execute binaries; override to skip them entirely)
sed -i 's/cross_compiling=maybe/cross_compiling=yes/' "$COREDIR/configure"

cd "$BUILD_DIR"
# Force re-configure so the new CFLAGS (containing -D__GLIBC__=0) are captured
rm -f Makefile
"$COREDIR/configure" \
        --host="$TARGET" \
        --prefix=/ \
        --disable-nls \
        --disable-rpath \
        --disable-libcap \
        --disable-gcc-warnings \
        --enable-cross-guesses=risky \
        --enable-no-install-program=timeout,stdbuf

# Patch gnulib's getprogname.c to add __OHOS__ fallback
# newlib doesn't provide program_invocation_short_name or other getprogname mechanisms
GNULIB_GETPROGNAME="$COREDIR/gnulib/lib/getprogname.c"
LIB_GETPROGNAME="$COREDIR/lib/getprogname.c"
if [ -f "$GNULIB_GETPROGNAME" ] && ! grep -q "__OHOS__" "$GNULIB_GETPROGNAME" 2>/dev/null; then
    # Insert OHOS fallback before the # elif _AIX line using awk
    awk '
        /^# elif _AIX/ && !ohos_inserted {
            print "/* OpenHobbyOS: newlib stub - returns fixed name for error messages */"
            print "# elif defined __OHOS__ || defined __OHOS_KERNEL__"
            print "  static const char progname[] = \"coreutils\";"
            print "  return progname;"
            ohos_inserted = 1
        }
        { print }
    ' "$GNULIB_GETPROGNAME" > "$GNULIB_GETPROGNAME.tmp" && mv "$GNULIB_GETPROGNAME.tmp" "$GNULIB_GETPROGNAME"
fi
# Create symlink if missing
if [ -f "$GNULIB_GETPROGNAME" ] && [ ! -e "$LIB_GETPROGNAME" ]; then
    ln -s "$GNULIB_GETPROGNAME" "$LIB_GETPROGNAME"
fi

# GL_CFLAG_GNULIB_WARNINGS leaks test source code under cross-compile; override it.
# STDDEF_NOT_IDEMPOTENT=1 in gnulib disables __need_wint_t handling, which breaks
# newlib's sys/_types.h; generate the header first, then re-enable it.
# GCC's i686-openhobbyos target is based on i686-linux-gnu so it pre-defines
# __GLIBC__=2 which causes gnulib to take glibc-specific code paths; override
# to 0 to use the generic (portable) fallbacks.
# MOUNTED_NOT_PORTED forces an #error in mountlist.c; stub it out so the
# function returns an empty list (fine for a single-root FS).
rm -f lib/libcoreutils.a lib/*.o lib/*/libcoreutils_a-*.o lib/libcoreutils_a-*.o
sed -i '/# *define MOUNTED_NOT_PORTED 1/s/1/0/' lib/config.h
sed -i '/^\/\* #undef HAVE_LCHOWN \*\/$/a #define HAVE_LCHOWN 1' lib/config.h
sed -i '/^\/\* #undef HAVE_DUP3 \*\/$/a #define HAVE_DUP3 1' lib/config.h 2>/dev/null || true
# Keep HAVE_DECL_PROGRAM_* at 0 since newlib doesn't provide these declarations
# The __OHOS__ fallback in getprogname.c will be used instead
# Disable glibc-specific sched_getaffinity which uses CPU_ALLOC macros not in newlib
sed -i 's/#define HAVE_SCHED_GETAFFINITY 1/#define HAVE_SCHED_GETAFFINITY 0/' lib/config.h
sed -i 's/#define HAVE_SCHED_GETAFFINITY_LIKE_GLIBC 1/#define HAVE_SCHED_GETAFFINITY_LIKE_GLIBC 0/' lib/config.h
# Fix newlib's sys/utime.h missing time_t include
SYSROOT_UTIME="$SYSROOT/i686-elf/include/sys/utime.h"
if [ -f "$SYSROOT_UTIME" ] && ! grep -q "#include <time.h>" "$SYSROOT_UTIME" 2>/dev/null; then
    sed -i '1a #include <time.h>' "$SYSROOT_UTIME"
fi
MAKE="make -f Makefile"
$MAKE -j1 lib/stddef.h GL_CFLAG_GNULIB_WARNINGS= 2>/dev/null || true
sed -i '/^    && !1$/s/!1/1/' lib/stddef.h

# Build the shared library first, then only the programs we need.
# Programs that use Linux-specific APIs (prctl, sig2str, str2sig) are excluded:
# timeout, env, kill, split, coreutils (multi-call binary).
# The list is matched against available make targets so missing programs
# (e.g. cmp) don't cause an error.
PROGRAMS="
  basename cat chmod cp cut date dd dirname
  echo expand false fmt fold head join link ln mkdir mv
  ls paste pr printf rm rmdir seq sleep sort sum sync
  tac tail tee touch tr true truncate uniq unlink wc yes
"

# Build all BUILT_SOURCES first so generated gnulib headers (error.h,
# stdcountof.h, etc.) exist before compilation starts.  Running make with
# no target defaults to 'all', which builds BUILT_SOURCES then recurses;
# ignore failures from the recursive step since we build programs individually.
$MAKE -j1 GL_CFLAG_GNULIB_WARNINGS= 2>/dev/null || true
$MAKE -j1 lib/libcoreutils.a GL_CFLAG_GNULIB_WARNINGS=
mkdir -p "$SYSROOT/bin"
OK_LIST=""
FAIL_LIST=""
for p in $PROGRAMS; do
  if $MAKE -j"$JOBS" GL_CFLAG_GNULIB_WARNINGS= "src/$p" 2>/dev/null; then
    OK_LIST="$OK_LIST $p"
    install -m 755 "src/$p" "$SYSROOT/bin/$p"
  else
    FAIL_LIST="$FAIL_LIST $p"
  fi
done

echo ""
echo "=== coreutils build summary ==="
echo "OK:   $OK_LIST"
echo "FAIL: $FAIL_LIST"
