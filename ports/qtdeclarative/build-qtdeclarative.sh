#!/usr/bin/env bash
# Build QtDeclarative (Qt6Qml, Qt6Quick, Qt6QmlModels) for OpenHobbyOS.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/qtdeclarative"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-openhobbyos}
QT_SRC="$ROOT/user/lib/graphics32/qtbase"
QTDECL_SRC="$ROOT/user/lib/graphics32/qtdeclarative"
PORT_DIR="$ROOT/ports/qtdeclarative"
HOST_INSTALL_DIR="$ROOT/build/ports/qt/qt-host"

mkdir -p "$BUILD_DIR" "$SYSROOT"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)
QT_SRC=$(cd "$QT_SRC" && pwd)
QTDECL_SRC=$(cd "$QTDECL_SRC" && pwd)
HOST_INSTALL_DIR=$(cd "$HOST_INSTALL_DIR" && pwd)

export PATH="$ROOT/toolchain/bin:$PATH"
export OPENHOBBYOS_SYSROOT="$SYSROOT"

echo "[qtdeclarative] Checking prerequisites..."

if [ ! -d "$QT_SRC" ]; then
    echo "ERROR: qtbase source not found at $QT_SRC"
    exit 1
fi

if [ ! -d "$SYSROOT/lib/cmake/Qt6" ]; then
    echo "ERROR: Cross-compiled Qt6 not found in sysroot."
    echo "  Build Qt6 first: make ports-qt"
    exit 1
fi

if [ ! -d "$HOST_INSTALL_DIR/lib/cmake/Qt6CoreTools" ]; then
    echo "ERROR: Host Qt6 tools not found."
    exit 1
fi

if [ ! -d "$QTDECL_SRC" ]; then
    echo "[qtdeclarative] qtdeclarative source not found at $QTDECL_SRC"
    echo "[qtdeclarative] Cloning from upstream..."
    git clone --depth 1 https://github.com/qt/qtdeclarative.git "$QTDECL_SRC" 2>&1 || {
        echo "ERROR: Failed to clone qtdeclarative. Clone it manually:"
        echo "  git clone https://github.com/qt/qtdeclarative.git $QTDECL_SRC"
        exit 1
    }
    echo "[qtdeclarative] cloned qtdeclarative at commit $(cd $QTDECL_SRC && git rev-parse HEAD)"
fi

PATCHES_DIR="$ROOT/patches"
echo "[qtdeclarative] Applying patches..."
(cd "$QTDECL_SRC" && if [ -f "$PATCHES_DIR/qtdeclarative-fixes.patch" ]; then
    git apply "$PATCHES_DIR/qtdeclarative-fixes.patch" 2>/dev/null || \
        echo "[qtdeclarative] (patch may already be applied)"
fi)

HOST_CMAKE_DIR="$HOST_INSTALL_DIR/lib/cmake"
if [ ! -d "$HOST_CMAKE_DIR" ]; then
    HOST_CMAKE_DIR=$(find "$HOST_INSTALL_DIR" -name "Qt6Config.cmake" -type f 2>/dev/null | head -1 | xargs dirname)
    HOST_CMAKE_DIR=$(dirname "$HOST_CMAKE_DIR" 2>/dev/null) || true
fi

# ---------------------------------------------------------------
# Auto-install: ensure missing bundled libs, cmake targets, and mkspecs
# are present in the sysroot (in case the qt build script didn't do it).
# ---------------------------------------------------------------
QT_CROSS_LIB="$ROOT/build/ports/qt/cross/lib"
echo "[qtdeclarative] Auto-installing missing Qt pieces..."

for lib in libQt6BundledFreetype.a libQt6BundledHarfbuzz.a libQt6BundledLibpng.a libQt6BundledPcre2.a; do
    if [ -f "$QT_CROSS_LIB/$lib" ] && [ ! -f "$SYSROOT/lib/$lib" ]; then
        cp -v "$QT_CROSS_LIB/$lib" "$SYSROOT/lib/$lib"
    fi
done
for prl in libQt6Core.prl libQt6Gui.prl libQt6Widgets.prl; do
    if [ -f "$QT_CROSS_LIB/$prl" ] && [ ! -f "$SYSROOT/lib/$prl" ]; then
        cp -v "$QT_CROSS_LIB/$prl" "$SYSROOT/lib/$prl"
    fi
done

for mod_dir in "$QT_CROSS_LIB/cmake"/*/; do
    mod=$(basename "$mod_dir")
    for pat in "*Targets.cmake" "*AdditionalTargetInfo.cmake"; do
        for src in "$mod_dir"/$pat; do
            [ -f "$src" ] || continue
            fname=$(basename "$src")
            dst="$SYSROOT/lib/cmake/$mod/$fname"
            [ -f "$dst" ] && continue
            mkdir -p "$(dirname "$dst")"
            cp -v "$src" "$dst"
        done
    done
done

if [ -d "$QT_SRC/mkspecs" ] && [ ! -d "$SYSROOT/mkspecs/linux-g++" ]; then
    echo "[qtdeclarative] Installing mkspecs from qtbase source..."
    for mkspec_dir in "$QT_SRC/mkspecs"/*/; do
        sub=$(basename "$mkspec_dir")
        case "$sub" in common|features|devices|dummy) continue ;; esac
        tgt="$SYSROOT/mkspecs/$sub"
        if [ ! -d "$tgt" ]; then
            mkdir -p "$tgt"
            cp -r "$mkspec_dir"/* "$tgt/"
        fi
    done
    for needed in common features; do
        if [ ! -d "$SYSROOT/mkspecs/$needed" ]; then
            cp -r "$QT_SRC/mkspecs/$needed" "$SYSROOT/mkspecs/$needed"
        fi
    done
fi

# Auto-install: copy versioned private header dirs (6.13.0/) from cross build
# Qt6::CorePrivate et al. reference sysroot/include/QtCore/6.13.0/ etc.
QT_CROSS_INCLUDE="$ROOT/build/ports/qt/cross/include"
for mod in QtCore QtGui QtWidgets; do
    if [ -d "$QT_CROSS_INCLUDE/$mod/6.13.0" ] && [ ! -d "$SYSROOT/include/$mod/6.13.0" ]; then
        echo "[qtdeclarative] Installing $mod/6.13.0/ headers..."
        mkdir -p "$SYSROOT/include/$mod"
        cp -r "$QT_CROSS_INCLUDE/$mod/6.13.0" "$SYSROOT/include/$mod/6.13.0"
    fi
    # Also copy any missing files (both .h and extensionless Qt wrapper headers like QString, QObject)
    for f in "$QT_CROSS_INCLUDE/$mod/"*; do
        [ -f "$f" ] || continue
        fname=$(basename "$f")
        if [ ! -f "$SYSROOT/include/$mod/$fname" ]; then
            cp -v "$f" "$SYSROOT/include/$mod/$fname"
        fi
    done
done

mkdir -p "$SYSROOT/plugins/platforms"
if [ -f "$ROOT/build/ports/qt/cross/plugins/platforms/libqopenhobbyos.so" ] && [ ! -f "$SYSROOT/plugins/platforms/libqopenhobbyos.so" ]; then
    cp -v "$ROOT/build/ports/qt/cross/plugins/platforms/libqopenhobbyos.so" "$SYSROOT/plugins/platforms/"
fi

# ---------------------------------------------------------------
# Pre-configure cleanup: remove spurious qtbase cmake dirs for
# unbuilt modules (Network, Sql, Test, Widgets, etc.) that
# build-qt.sh may have left behind.
# ---------------------------------------------------------------
echo "[qtdeclarative] Cleaning up spurious qtbase cmake dirs..."
# Only remove cmake dirs for qtbase modules that weren't actually built,
# NOT qtdeclarative modules (Qml, Quick, Labs, etc.) which we just installed.
for mod_dir in "$SYSROOT/lib/cmake"/*/; do
    mod=$(basename "$mod_dir")
    # Never remove qtdeclarative-owned modules
    case "$mod" in
        Qt6Qml*|Qt6Quick*|Qt6Labs*) continue ;;
    esac
    # Keep infrastructure and core modules
    case "$mod" in
        Qt6|Qt6BuildInternals|Qt6HostInfo|Qt6Platform|Qt6PlatformSupportPrivate) continue ;;
        Qt6FbSupportPrivate|Qt6DeviceDiscoverySupportPrivate|Qt6ServiceSupportPrivate) continue ;;
        Qt6ThemeSupportPrivate|Qt6VulkanSupportPrivate|Qt6PlatformCompositorSupportPrivate) continue ;;
        Qt6Bundled*|Qt6CorePrivate|Qt6GuiPrivate) continue ;;
        Qt6CoreTools|Qt6GuiTools|Qt6Gui|Qt6Core) continue ;;
    esac
    mod_base=$(echo "$mod" | sed 's/^Qt6//; s/Private$//; s/Tools$//')
    so_path="$SYSROOT/lib/libQt6${mod_base}.so"
    if [ ! -f "$so_path" ] && [ ! -f "${so_path}.6" ] && [ ! -f "${so_path}.6.13.0" ]; then
        echo "  Removing spurious qtbase: $mod"
        rm -rf "$SYSROOT/lib/cmake/$mod"
    fi
done

# ---------------------------------------------------------------
# Cross-compile Qt6Qml, Qt6Quick, Qt6QmlModels
# ---------------------------------------------------------------
CROSS_MARKER="$BUILD_DIR/.cross_built"
if [ ! -f "$CROSS_MARKER" ]; then
    echo "[qtdeclarative] Configuring cross-compile..."
    rm -rf "$BUILD_DIR/CMakeCache.txt" "$BUILD_DIR/CMakeFiles" 2>/dev/null || true

    cmake -S "$QTDECL_SRC" -B "$BUILD_DIR" \
        -DCMAKE_TOOLCHAIN_FILE="$ROOT/ports/qt/openhobbyos-toolchain.cmake" \
        -DCMAKE_MODULE_PATH="$ROOT/ports/qt" \
        -DOPENHOBBYOS=ON \
        -DQT_QPA_PLATFORMS="openhobbyos" \
        -DFEATURE_qml_network=OFF \
        -DFEATURE_qml_jit=OFF \
        -DFEATURE_qml_animation=ON \
        -DFEATURE_qml_debug=OFF \
        -DFEATURE_qml_profiler=OFF \
        -DFEATURE_qml_preview=OFF \
        -DFEATURE_qml_worker_script=ON \
        -DFEATURE_quick_shadereffect=OFF \
        -DFEATURE_quick_particles=OFF \
        -DFEATURE_quick_path=ON \
        -DFEATURE_quick_tableview=ON \
        -DFEATURE_quick_treeview=OFF \
        -DFEATURE_sbom=OFF \
        -DQT_BUILD_EXAMPLES=OFF \
        -DQT_BUILD_TESTS=OFF \
        -DQT_FORCE_BUILD_TOOLS=ON \
        -DQT_HOST_PATH="$HOST_INSTALL_DIR" \
        -DQT_HOST_PATH_CMAKE_DIR="$HOST_CMAKE_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$SYSROOT" \
        -DCMAKE_FIND_ROOT_PATH="$SYSROOT" \
        -DCMAKE_PROJECT_INCLUDE="$PORT_DIR/QmlToolStubs.cmake" \
        2>&1

    # qml_stub_tool.sh handles generating qml_qmltyperegistrations.cpp and other
    # generated files automatically — no manual patching or copying needed.

    echo "[qtdeclarative] Building Qt6Qml..."
    cmake --build "$BUILD_DIR" --parallel "$(nproc)" --target Qml 2>&1 || \
    cmake --build "$BUILD_DIR" --parallel "$(nproc)" --target qml 2>&1 || \
    cmake --build "$BUILD_DIR" --parallel "$(nproc)" --target all 2>&1

    echo "[qtdeclarative] Building Qt6QmlModels..."
    cmake --build "$BUILD_DIR" --parallel "$(nproc)" --target QmlModels 2>&1

    echo "[qtdeclarative] Building Qt6Quick..."
    cmake --build "$BUILD_DIR" --parallel "$(nproc)" --target Quick 2>&1

    echo "[qtdeclarative] Installing..."
    # Install all components (not just Devel) to ensure shared libraries,
    # cmake configs, and headers all get installed to the sysroot.
    cmake --install "$BUILD_DIR" 2>&1 || true
    # Manual fallback: copy any shared libs that the install may have missed
    # (some static lib targets fail install when not built)
    for lib in "$BUILD_DIR/lib/"*.so*; do
        [ -f "$lib" ] || continue
        fname=$(basename "$lib")
        if [ ! -f "$SYSROOT/lib/$fname" ]; then
            cp -v "$lib" "$SYSROOT/lib/$fname"
        fi
    done

    touch "$CROSS_MARKER"
else
    echo "[qtdeclarative] Already built, re-installing..."
    cmake --install "$BUILD_DIR" 2>/dev/null || true
fi

# Post-install: fix build-directory paths in installed cmake targets
echo "[qtdeclarative] Fixing build-directory paths in cmake targets..."
for targets_file in $(find "$SYSROOT/lib/cmake" -name "*Targets.cmake" -type f 2>/dev/null); do
    if grep -q "$BUILD_DIR" "$targets_file" 2>/dev/null; then
        sed -i "s|$BUILD_DIR|\${_IMPORT_PREFIX}|g" "$targets_file"
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

touch "$BUILD_DIR/.built"
echo "[qtdeclarative] QtDeclarative built and installed to $SYSROOT"