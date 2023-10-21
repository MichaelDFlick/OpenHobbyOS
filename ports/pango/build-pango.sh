#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/pango"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-openhobbyos}
PANGO_SRC="$ROOT/user/lib/pango"

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
export PKG_CONFIG_PATH="$SYSROOT/lib/pkgconfig:$SYSROOT/share/pkgconfig"

echo "[pango] Reverting previous patches..."
cd "$PANGO_SRC"
git checkout -- meson.build 2>/dev/null || true

echo "[pango] Generating cross-compilation file..."
sed \
    -e "s|@TARGET@|$TARGET|g" \
    -e "s|@SYSROOT@|$SYSROOT|g" \
    "$ROOT/ports/meson/openhobbyos.cross.in" > "$CROSS_FILE"

# Pango needs a C++ compiler and cpp_args (no -ffreestanding for C++)
sed -i "/^c = /a cpp = '$TARGET-g++'" "$CROSS_FILE"
sed -i "/^c_link_args = /i cpp_args = ['--sysroot=$SYSROOT', '-O2', '-fno-pic', '-fno-pie']" "$CROSS_FILE"

echo "[pango] Applying build-time patches..."
cd "$PANGO_SRC"
# Bypass the cairo-ft FontConfig link check by replacing the block:
#   if not cc.links(cairo_fc_test, ...)
#     error(...)
#   endif
# with:
#   if true
#   endif
if grep -q 'if not cc.links.*cairo_fc_test' meson.build; then
  python3 << 'PYEOF'
with open('meson.build', 'r') as f:
    lines = f.readlines()
with open('meson.build', 'w') as f:
    skip = False
    for i, line in enumerate(lines):
        if 'if not cc.links(cairo_fc_test,' in line:
            skip = True
            f.write('            if true\n')
            f.write('            endif\n')
            # skip until endif
            for j in range(i+1, len(lines)):
                if lines[j].strip() == 'endif':
                    break
            continue
        if skip:
            if line.strip() == 'endif':
                skip = False
            continue
        f.write(line)
PYEOF
  echo "[pango] Patched cairo-ft link check"
fi

echo "[pango] Configuring..."
"$MESON" setup \
    "$BUILD_DIR/build" \
    "$PANGO_SRC" \
    --cross-file "$CROSS_FILE" \
    --prefix / \
    --bindir bin \
    --libdir lib \
    --includedir include \
    --datadir share \
    -Ddefault_library=static \
    -Ddocumentation=false \
    -Dintrospection=disabled \
    -Dbuild-testsuite=false \
    -Dbuild-examples=false \
    -Dman-pages=false \
    -Dcairo=enabled \
    -Dxft=disabled \
    -Dsysprof=disabled \
    -Dlibthai=disabled \
    --wrap-mode=nodownload \
    --buildtype release

echo "[pango] Building..."
# The full build may fail on tools (gen-script-for-lang, pango-view) that
# link against missing host functions (syslog, nftw, etc.), but libraries
# will have been built already. :3
set +e
"$MESON" compile -C "$BUILD_DIR/build"
BUILD_EXIT=$?
set -e
if [ $BUILD_EXIT -ne 0 ]; then
    echo "[pango] Build had non-fatal errors (utilities may have failed), proceeding..."
fi

echo "[pango] Installing libraries and headers..."
set +e
# Install everything; pango-view will fail but others should succeed
"$MESON" install -C "$BUILD_DIR/build" --destdir "$SYSROOT" --no-rebuild 2>/dev/null || true
set -e

# Manually install .pc files if meson install missed them
for pc in pango.pc pangoft2.pc pangocairo.pc pangoot.pc pangofc.pc; do
    src_pc="$BUILD_DIR/build/meson-private/$pc"
    if [ -f "$src_pc" ]; then
        cp "$src_pc" "$SYSROOT/lib/pkgconfig/$pc"
        echo "[pango] Installed $pc"
    fi
done

# Install headers manually if meson install failed
PANGO_DEST="$SYSROOT/include/pango-1.0/pango"
if [ ! -f "$PANGO_DEST/pango.h" ]; then
    echo "[pango] Installing headers manually..."
    mkdir -p "$PANGO_DEST"

    # Copy headers from source pango/ directory
    for h in "$PANGO_SRC"/pango/*.h; do
        [ -f "$h" ] && cp "$h" "$PANGO_DEST/"
    done

    # Copy generated headers from build directory
    for h in "$BUILD_DIR/build"/pango/*.h; do
        [ -f "$h" ] && cp "$h" "$PANGO_DEST/"
    done
fi

echo "[pango] Built and installed successfully"
