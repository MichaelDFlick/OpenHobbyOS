#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/freetype"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}

mkdir -p "$BUILD_DIR" "$SYSROOT/include/freetype/config" "$SYSROOT/lib" "$SYSROOT/lib/pkgconfig"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)

CC=${CC:-gcc}
CFLAGS=${CFLAGS:--std=gnu11 -m32 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fno-builtin -O2 -Wall -Wextra -mno-sse -mno-sse2 -mfpmath=387}

FREETYPE_SRC="$ROOT/user/lib/freetype"
OVERRIDES="$ROOT/ports/freetype"

FREETYPE_CFLAGS="$CFLAGS -DFT2_BUILD_LIBRARY"
FREETYPE_CFLAGS="$FREETYPE_CFLAGS -I$OVERRIDES -I$FREETYPE_SRC/include -I$ROOT/include"

SOURCES=(
    "$FREETYPE_SRC/src/base/ftbase.c"
    "$FREETYPE_SRC/src/base/ftbitmap.c"
    "$FREETYPE_SRC/src/base/ftinit.c"
    "$FREETYPE_SRC/src/base/ftmm.c"
    "$FREETYPE_SRC/src/base/ftsynth.c"
    "$OVERRIDES/ftsystem_kernel.c"
    "$OVERRIDES/ftdebug_kernel.c"
    "$FREETYPE_SRC/src/sfnt/sfnt.c"
    "$FREETYPE_SRC/src/truetype/truetype.c"
    "$FREETYPE_SRC/src/psnames/psnames.c"
    "$FREETYPE_SRC/src/psaux/psaux.c"
    "$FREETYPE_SRC/src/smooth/smooth.c"
    "$FREETYPE_SRC/src/raster/raster.c"
)

OBJECTS=()
for src in "${SOURCES[@]}"; do
    basename=$(basename "$src" .c)
    obj="$BUILD_DIR/${basename}_ft.o"
    echo "  CC      ${src#$ROOT/}"
    $CC $FREETYPE_CFLAGS -c "$src" -o "$obj"
    OBJECTS+=("$obj")
done

echo "  AR      libfreetype.a"
${AR:-ar} rcs "$BUILD_DIR/libfreetype.a" "${OBJECTS[@]}"
${RANLIB:-ranlib} "$BUILD_DIR/libfreetype.a" 2>/dev/null || true

install -m 644 "$BUILD_DIR/libfreetype.a" "$SYSROOT/lib/libfreetype.a"

for header in $(find "$FREETYPE_SRC/include/freetype" -name '*.h' -type f); do
    rel="${header#$FREETYPE_SRC/include/}"
    mkdir -p "$SYSROOT/include/$(dirname "$rel")"
    install -m 644 "$header" "$SYSROOT/include/$rel"
done

for header in $(find "$OVERRIDES/freetype" -name '*.h' -type f); do
    rel="${header#$OVERRIDES/}"
    mkdir -p "$SYSROOT/include/$(dirname "$rel")"
    install -m 644 "$header" "$SYSROOT/include/$rel"
done

install -m 644 "$FREETYPE_SRC/include/ft2build.h" "$SYSROOT/include/ft2build.h"

cat > "$SYSROOT/lib/pkgconfig/freetype2.pc" <<'EOF'
prefix=/
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: freetype2
Description: FreeType 2 font engine
Version: 2.14.3
Cflags: -I${includedir}
Libs: -L${libdir} -lfreetype
EOF

echo "[freetype] built and installed to $SYSROOT"
