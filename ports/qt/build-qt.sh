#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/qt"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-openhobbyos}

QT_SRC="$ROOT/user/lib/graphics32/qtbase"
PORT_DIR="$ROOT/ports/qt"
PATCHES_DIR="$ROOT/patches"
HOST_BUILD_DIR="$BUILD_DIR/host"
CROSS_BUILD_DIR="$BUILD_DIR/cross"
HOST_INSTALL_DIR="$BUILD_DIR/qt-host"

mkdir -p "$BUILD_DIR" "$SYSROOT" "$HOST_BUILD_DIR" "$CROSS_BUILD_DIR" "$HOST_INSTALL_DIR"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
HOST_BUILD_DIR=$(cd "$HOST_BUILD_DIR" && pwd)
CROSS_BUILD_DIR=$(cd "$CROSS_BUILD_DIR" && pwd)
HOST_INSTALL_DIR=$(cd "$HOST_INSTALL_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)

export PATH="$ROOT/toolchain/bin:$PATH"
export OPENHOBBYOS_SYSROOT="$SYSROOT"

echo "[qt] Verifying dependencies..."
if [ ! -f "$SYSROOT/lib/pkgconfig/pixman-1.pc" ]; then
    echo "ERROR: pixman must be built first (run 'make ports-pixman')"
    exit 1
fi
if [ ! -d "$SYSROOT/include/xnx" ]; then
    echo "ERROR: xnx must be built first (run 'make ports-xnx')"
    exit 1
fi
if [ ! -f "$SYSROOT/lib/pkgconfig/freetype2.pc" ]; then
    echo "ERROR: freetype must be built first (run 'make ports-freetype')"
    exit 1
fi

echo "[qt] Applying patches to qtbase submodule..."
cd "$QT_SRC"
if [ -f "$PATCHES_DIR/qtbase-add-openhobbyos-platform.patch" ]; then
    git apply "$PATCHES_DIR/qtbase-add-openhobbyos-platform.patch" 2>/dev/null || \
        echo "[qt] (patch may already be applied)"
fi

echo "[qt] Installing OpenHobbyOS QPA platform sources..."
PLATFORM_DIR="$QT_SRC/src/plugins/platforms/openhobbyos"
mkdir -p "$PLATFORM_DIR"
cp -v "$PORT_DIR"/openhobbyos/*.cpp \
      "$PORT_DIR"/openhobbyos/*.h \
      "$PORT_DIR"/openhobbyos/*.json \
      "$PORT_DIR"/openhobbyos/CMakeLists.txt \
      "$PLATFORM_DIR/"

# ---------------------------------------------------------------
# Stage 1: Build host Qt tools natively
# ---------------------------------------------------------------
if [ ! -f "$HOST_INSTALL_DIR/lib/cmake/Qt6CoreTools/Qt6CoreToolsConfig.cmake" ]; then
    echo "[qt] Stage 1: Building host Qt fully (minimal config)..."
echo "[qt]   This will take a while (building ~20-30 min on typical hardware)"
    rm -rf "$HOST_BUILD_DIR/CMakeCache.txt" "$HOST_BUILD_DIR/CMakeFiles" 2>/dev/null || true

    cmake -S "$QT_SRC" -B "$HOST_BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DQT_BUILD_EXAMPLES=OFF \
        -DQT_BUILD_TESTS=OFF \
        -DBUILD_SHARED_LIBS=ON \
        -DFEATURE_linuxfb=ON \
        -DFEATURE_xcb=OFF \
        -DFEATURE_wayland=OFF \
        -DFEATURE_eglfs=OFF \
        -DFEATURE_directfb=OFF \
        -DFEATURE_opengl=OFF \
        -DFEATURE_vulkan=OFF \
        -DFEATURE_system_pixman=OFF \
        -DFEATURE_fontconfig=OFF \
        -DFEATURE_dbus=OFF \
        -DFEATURE_sql=OFF \
        -DFEATURE_network=OFF \
        -DFEATURE_xml=OFF \
        -DCMAKE_INSTALL_PREFIX="$HOST_INSTALL_DIR"

    cmake --build "$HOST_BUILD_DIR" --parallel "$(nproc)"
    cmake --install "$HOST_BUILD_DIR" --prefix "$HOST_INSTALL_DIR"
    echo "[qt] Stage 1 complete: host Qt installed to $HOST_INSTALL_DIR"
else
    echo "[qt] Stage 1: host Qt already built, skipping."
fi

# ---------------------------------------------------------------
# Ensure Qt6HostInfo cmake module is available
# ---------------------------------------------------------------
HOST_INFO_SRC="$HOST_BUILD_DIR/lib/cmake/Qt6HostInfo"
HOST_INFO_DST="$HOST_INSTALL_DIR/lib/cmake/Qt6HostInfo"
if [ -d "$HOST_INFO_SRC" ] && [ ! -f "$HOST_INFO_DST/Qt6HostInfoConfig.cmake" ]; then
    mkdir -p "$HOST_INFO_DST"
    cp -r "$HOST_INFO_SRC/"* "$HOST_INFO_DST/"
    echo "[qt]   installed Qt6HostInfo cmake module"
fi

# ---------------------------------------------------------------
# Stage 2: Cross-compile OpenHobbyOS QPA plugin
# ---------------------------------------------------------------
CROSS_MARKER="$CROSS_BUILD_DIR/.qpa_built"
if [ ! -f "$CROSS_MARKER" ]; then
    echo "[qt] Stage 2: Cross-compiling OpenHobbyOS QPA plugin..."

    # Compute host cmake dir (handle multiarch paths)
    HOST_CMAKE_DIR="$HOST_INSTALL_DIR/lib/cmake"
    if [ ! -d "$HOST_CMAKE_DIR" ]; then
        HOST_CMAKE_DIR=$(find "$HOST_INSTALL_DIR" -name "Qt6Config.cmake" -type f 2>/dev/null | head -1 | xargs dirname)
        HOST_CMAKE_DIR=$(dirname "$HOST_CMAKE_DIR" 2>/dev/null) || true
    fi
    if [ ! -d "$HOST_CMAKE_DIR" ]; then
        echo "ERROR: could not find Qt6 cmake dir under $HOST_INSTALL_DIR"
        exit 1
    fi

    rm -rf "$CROSS_BUILD_DIR/CMakeCache.txt" "$CROSS_BUILD_DIR/CMakeFiles" 2>/dev/null || true

    cmake -S "$QT_SRC" -B "$CROSS_BUILD_DIR" \
        -DCMAKE_TOOLCHAIN_FILE="$PORT_DIR/openhobbyos-toolchain.cmake" \
        -DOPENHOBBYOS=ON \
        -DQT_QPA_PLATFORMS="openhobbyos" \
        -DFEATURE_linuxfb=OFF \
        -DFEATURE_xcb=OFF \
        -DFEATURE_wayland=OFF \
        -DFEATURE_eglfs=OFF \
        -DFEATURE_directfb=OFF \
        -DFEATURE_opengl=OFF \
        -DINPUT_opengl=no \
        -DFEATURE_vulkan=OFF \
        -DFEATURE_system_freetype=OFF \
        -DFEATURE_system_pixman=ON \
        -DQT_BUILD_EXAMPLES=OFF \
        -DQT_BUILD_TESTS=OFF \
        -DQT_FORCE_BUILD_TOOLS=ON \
        -DQT_HOST_PATH="$HOST_INSTALL_DIR" \
        -DQT_HOST_PATH_CMAKE_DIR="$HOST_CMAKE_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$SYSROOT"

    echo "[qt] Building..."
    cmake --build "$CROSS_BUILD_DIR" --parallel "$(nproc)" --target QOpenHobbyOSIntegrationPlugin

    echo "[qt] Installing..."
    cmake --install "$CROSS_BUILD_DIR" --component Devel

    touch "$CROSS_MARKER"
else
    echo "[qt] Stage 2: QPA plugin already built, re-running install + post-install..."
    cmake --install "$CROSS_BUILD_DIR" --component Devel 2>/dev/null || true
fi

# ---------------------------------------------------------------
# Post-install: install missing bundled libs, cmake targets, and mkspecs
# that the minimal --component Devel install does not cover.
# ---------------------------------------------------------------
echo "[qt] Post-install: installing bundled static libraries..."
for lib in libQt6BundledFreetype.a libQt6BundledHarfbuzz.a libQt6BundledLibpng.a libQt6BundledPcre2.a; do
    if [ -f "$CROSS_BUILD_DIR/lib/$lib" ] && [ ! -f "$SYSROOT/lib/$lib" ]; then
        cp -v "$CROSS_BUILD_DIR/lib/$lib" "$SYSROOT/lib/$lib"
    fi
done
for prl in libQt6Core.prl libQt6Gui.prl libQt6Widgets.prl; do
    if [ -f "$CROSS_BUILD_DIR/lib/$prl" ] && [ ! -f "$SYSROOT/lib/$prl" ]; then
        cp -v "$CROSS_BUILD_DIR/lib/$prl" "$SYSROOT/lib/$prl"
    fi
done

echo "[qt] Post-install: installing missing cmake target files..."
for mod_dir in "$CROSS_BUILD_DIR/lib/cmake"/*/; do
    mod=$(basename "$mod_dir")
    for pat in "*Targets.cmake" "*AdditionalTargetInfo.cmake"; do
        for src in "$mod_dir"/$pat; do
            [ -f "$src" ] || continue
            fname=$(basename "$src")
            dst="$SYSROOT/lib/cmake/$mod/$fname"
            if [ ! -f "$dst" ]; then
                cp -v "$src" "$dst"
            fi
        done
    done
done

# Fix any build-directory paths in the copied targets files
# (replace absolute cross-build-dir paths with relocatable ${_IMPORT_PREFIX})
echo "[qt] Post-install: fixing build-directory paths in cmake targets..."
for targets_file in $(find "$SYSROOT/lib/cmake" -name "*Targets.cmake" -type f 2>/dev/null); do
    if grep -q "$CROSS_BUILD_DIR" "$targets_file" 2>/dev/null; then
        sed -i "s|$CROSS_BUILD_DIR|\${_IMPORT_PREFIX}|g" "$targets_file"
        if ! grep -q "get_filename_component(_IMPORT_PREFIX" "$targets_file"; then
            sed -i '/^# Create imported target/i\
# Compute the installation prefix relative to this file.\
get_filename_component(_IMPORT_PREFIX "${CMAKE_CURRENT_LIST_FILE}" PATH)\
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)\
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)\
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)\
if(_IMPORT_PREFIX STREQUAL "/")\
  set(_IMPORT_PREFIX "")\
endif()' "$targets_file"
        fi
    fi
done

# Remove cmake dirs for modules that weren't actually built (no .so in sysroot).
# These targets have relocatable paths now but the libraries/headers don't exist,
# causing cmake errors when consumers try to use them.
echo "[qt] Post-install: removing cmake dirs for unbuilt modules..."
for mod_dir in "$SYSROOT/lib/cmake"/*/; do
    mod=$(basename "$mod_dir")
    # Never remove modules that come from qtdeclarative (Qml, Quick, Labs)
    case "$mod" in
        Qt6Qml*|Qt6Quick*|Qt6Labs*) continue ;;
    esac
    # Keep umbrella and infrastructure modules
    case "$mod" in
        Qt6|Qt6BuildInternals|Qt6HostInfo|Qt6Platform|Qt6PlatformSupportPrivate) continue ;;
        Qt6FbSupportPrivate|Qt6DeviceDiscoverySupportPrivate|Qt6ServiceSupportPrivate) continue ;;
        Qt6ThemeSupportPrivate|Qt6VulkanSupportPrivate|Qt6PlatformCompositorSupportPrivate) continue ;;
        Qt6Bundled*|Qt6CorePrivate|Qt6GuiPrivate) continue ;;
        Qt6CoreTools|Qt6GuiTools) continue ;;
    esac
    # Extract module name: strip Qt6 prefix and Private/Tools suffix
    mod_base=$(echo "$mod" | sed 's/^Qt6//; s/Private$//; s/Tools$//')
    # Check if corresponding .so exists
    so_path="$SYSROOT/lib/libQt6${mod_base}.so"
    if [ ! -f "$so_path" ] && [ ! -f "${so_path}.6" ] && [ ! -f "${so_path}.6.13.0" ]; then
        echo "  Removing unbuilt module: $mod"
        rm -rf "$SYSROOT/lib/cmake/$mod"
    fi
done

echo "[qt] Post-install: installing mkspecs from source tree..."
if [ -d "$QT_SRC/mkspecs" ]; then
    for mkspec_dir in "$QT_SRC/mkspecs"/*/; do
        sub=$(basename "$mkspec_dir")
        # Skip common, features, devices, dummy — those are included by reference
        case "$sub" in common|features|devices|dummy) continue ;; esac
        tgt="$SYSROOT/mkspecs/$sub"
        if [ ! -d "$tgt" ]; then
            mkdir -p "$tgt"
            cp -r "$mkspec_dir"/* "$tgt/"
            echo "  mkspec: $sub"
        fi
    done
    # Ensure common/ and features/ are always present
    for needed in common features; do
        if [ ! -d "$SYSROOT/mkspecs/$needed" ]; then
            cp -r "$QT_SRC/mkspecs/$needed" "$SYSROOT/mkspecs/$needed"
            echo "  mkspec: $needed (needed by platform specs)"
        fi
    done
fi

# Install platform plugin .so
mkdir -p "$SYSROOT/plugins/platforms"
if [ -f "$CROSS_BUILD_DIR/plugins/platforms/libqopenhobbyos.so" ]; then
    cp -v "$CROSS_BUILD_DIR/plugins/platforms/libqopenhobbyos.so" "$SYSROOT/plugins/platforms/"
fi

# Patch SBOM helpers to tolerate SPDX external document ref hash changes
# (needed when qtdeclarative builds against an already-installed qtbase that
# has a different git HEAD than when qt was built).
SBOM_FILE="$SYSROOT/lib/cmake/Qt6/QtPublicSbomExternalReferenceHelpers.cmake"
if [ -f "$SBOM_FILE" ] && grep -q "FATAL_ERROR.*already has an associated SPDX v2" "$SBOM_FILE"; then
    sed -i 's/message(FATAL_ERROR.*already has an associated SPDX v2/message(WARNING "&/' "$SBOM_FILE"
    echo "[qt] Patched SBOM external reference helper to avoid duplicate ref fatal error"
fi

touch "$BUILD_DIR/.built"
echo "[qt] OpenHobbyOS QPA plugin built and installed to $SYSROOT"
