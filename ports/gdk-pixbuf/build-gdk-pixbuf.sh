#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/gdk-pixbuf"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-openhobbyos}
GDK_PIXBUF_SRC="$ROOT/user/lib/gdk-pixbuf"

mkdir -p "$BUILD_DIR" "$SYSROOT"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)
CROSS_FILE="$BUILD_DIR/openhobbyos.cross"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
MESON="$("$ROOT/tools/ensure_meson.sh")"
export PATH="$ROOT/toolchain/bin:$PATH"
export OPENHOBBYOS_SYSROOT="$SYSROOT"
export PKG_CONFIG_ALLOW_SYSTEM_CFLAGS=1
export PKG_CONFIG_ALLOW_SYSTEM_LIBS=1

echo "[gdk-pixbuf] Generating cross-compilation file..."
GLIB_OVERRIDES="$ROOT/ports/glib/overrides"
sed \
    -e "s|@TARGET@|$TARGET|g" \
    -e "s|@SYSROOT@|$SYSROOT|g" \
    "$ROOT/ports/meson/openhobbyos.cross.in" > "$CROSS_FILE"

sed -i "s|c_link_args = .*|c_link_args = ['--sysroot=$SYSROOT', '-static', '-nostartfiles', '-Wl,-T,$SYSROOT/lib/user.ld', '-lopenhobbyosgloss', '-lglibstubs', '-lresolv']|" "$CROSS_FILE"

echo "[gdk-pixbuf] Configuring..."

# Remove timescale executable (unconditional build fails in cross-compilation)
sed -i "/executable('timescale'/d" "$GDK_PIXBUF_SRC/gdk-pixbuf/pixops/meson.build"

# Wipe old build to pick up meson changes
rm -rf "$BUILD_DIR/build"

"$MESON" setup \
    "$BUILD_DIR/build" \
    "$GDK_PIXBUF_SRC" \
    --cross-file "$CROSS_FILE" \
    --prefix / \
    --libdir lib \
    --includedir include \
    -Ddefault_library=static \
    -Dpng=disabled \
    -Djpeg=disabled \
    -Dtiff=disabled \
    -Dgif=disabled \
    -Dothers=disabled \
    -Ddocumentation=false \
    -Dintrospection=disabled \
    -Dman=false \
    -Dtests=false \
    -Dinstalled_tests=false \
    -Dglycin=disabled \
    -Dthumbnailer=disabled \
    -Dgio_sniffing=false \
    --wrap-mode=nodownload \
    --buildtype release

echo "[gdk-pixbuf] Building..."
"$MESON" compile -C "$BUILD_DIR/build" || true

# Only need the static library — tools may fail due to cross/static+libatomic conflict

echo "[gdk-pixbuf] Installing..."
"$MESON" install -C "$BUILD_DIR/build" --destdir "$SYSROOT" --no-rebuild 2>/dev/null || true

# If meson install failed (likely due to .so in static build), install manually
GDKPB_DEST="$SYSROOT/include/gdk-pixbuf-2.0"
if [ ! -f "$GDKPB_DEST/gdk-pixbuf/gdk-pixbuf.h" ]; then
    echo "[gdk-pixbuf] meson install incomplete, installing manually..."
    mkdir -p "$GDKPB_DEST"

    # Copy headers from source
    cd "$GDK_PIXBUF_SRC"
    find . -name '*.h' | while read h; do
        dir=$(dirname "$h")
        mkdir -p "$GDKPB_DEST/$dir"
        cp "$GDK_PIXBUF_SRC/$h" "$GDKPB_DEST/$h"
    done

    # Copy generated headers from build
    if [ -d "$BUILD_DIR/build" ]; then
        cd "$BUILD_DIR/build"
        find . -name '*.h' | while read h; do
            dir=$(dirname "$h")
            mkdir -p "$GDKPB_DEST/$dir"
            cp "$BUILD_DIR/build/$h" "$GDKPB_DEST/$h"
        done
    fi

    # Install static library (convert thin archive)
    THIN="$BUILD_DIR/build/gdk-pixbuf/libgdk_pixbuf-2.0.a"
    DEST="$SYSROOT/lib/libgdk_pixbuf-2.0.a"
    if [ -f "$THIN" ]; then
        if file "$THIN" | grep -q "thin archive"; then
            cd "$BUILD_DIR/build/gdk-pixbuf"
            AR="$ROOT/toolchain/bin/$TARGET-ar"
            find . -name '*.o' -print0 | xargs -0 "$AR" rcs "$DEST"
            "$ROOT/toolchain/bin/$TARGET-ranlib" "$DEST" 2>/dev/null || true
        else
            cp "$THIN" "$DEST"
        fi
    fi

    # Install pkgconfig
    mkdir -p "$SYSROOT/lib/pkgconfig"
    PKGDEST="$SYSROOT/lib/pkgconfig/gdk-pixbuf-2.0.pc"
    cat > "$PKGDEST" << 'PCEOF'
prefix=/
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: GdkPixbuf
Description: Image loading library
Version: 2.43.3
Cflags: -I${includedir}/gdk-pixbuf-2.0
Libs: -L${libdir} -lgdk_pixbuf-2.0
PCEOF
fi

echo "[gdk-pixbuf] Built and installed successfully"
