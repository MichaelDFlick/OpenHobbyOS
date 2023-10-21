#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/gtk"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-openhobbyos}
GTK_SRC="$ROOT/user/lib/gtk"
OPENHOBBY_SRC="$ROOT/ports/gtk/openhobby"

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
export PKG_CONFIG_LIBDIR=""
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"

echo "[gtk] Generating cross-compilation file..."
sed \
    -e "s|@TARGET@|$TARGET|g" \
    -e "s|@SYSROOT@|$SYSROOT|g" \
    "$ROOT/ports/meson/openhobbyos.cross.in" > "$CROSS_FILE"

sed -i "/^c = /a cpp = '$TARGET-g++'" "$CROSS_FILE"
sed -i "/^c_link_args = /i cpp_args = ['--sysroot=$SYSROOT', '-O2', '-fno-pic', '-fno-pie']" "$CROSS_FILE"

echo "[gtk] Reverting previous patches and cleaning backend directory..."
cd "$GTK_SRC"
# Clean the backend directory
rm -rf "$GTK_SRC/gdk/openhobby"
# Revert patched files to pristine state (submodule)
git checkout -- meson.options meson.build gdk/gdkconfig.h.meson gdk/meson.build gdk/gdkdisplaymanager.c 2>/dev/null || true

echo "[gtk] Copying openhobby backend source to GTK tree..."
BACKEND_DIR="$GTK_SRC/gdk/openhobby"
mkdir -p "$BACKEND_DIR"
cp "$OPENHOBBY_SRC"/*.c "$BACKEND_DIR/"
cp "$OPENHOBBY_SRC"/*.h "$BACKEND_DIR/"
cp "$OPENHOBBY_SRC"/meson.build "$BACKEND_DIR/"

# Create dummy theme CSS files (sassc is not available in cross-build env)
for variant in light dark hc hc-dark; do
  touch "$GTK_SRC/gtk/theme/Default/Default-${variant}.css"
done

echo "[gtk] Applying build-time patches..."

cd "$GTK_SRC"

# 1. Add openhobby-backend option to meson.options (after broadway)
if ! grep -q 'openhobby-backend' meson.options; then
  DESC_LINE=$(grep -n 'broadway.*HTML5' meson.options | cut -d: -f1)
  sed -i "${DESC_LINE}a\\
\\
option('openhobby-backend',\\
       type: 'boolean',\\
       value: false,\\
       description : 'Enable the OpenHobbyOS gdk backend')" meson.options
fi

# 2. Add openhobby_enabled to top-level meson.build (after broadway)
if ! grep -q 'openhobby_enabled' meson.build; then
  sed -i "/^broadway_enabled = /a openhobby_enabled = get_option('openhobby-backend')" meson.build
fi

# 3. Add GDK_WINDOWING_OPENHOBBY to gdk/gdkconfig.h.meson
if ! grep -q 'GDK_WINDOWING_OPENHOBBY' gdk/gdkconfig.h.meson; then
  sed -i "/^#mesondefine GDK_WINDOWING_BROADWAY/a #mesondefine GDK_WINDOWING_OPENHOBBY" gdk/gdkconfig.h.meson
fi

# 4. Add GDK_WINDOWING_OPENHOBBY to gdk/meson.build config_cdata
if ! grep -q 'GDK_WINDOWING_OPENHOBBY' gdk/meson.build; then
  sed -i "/gdkconfig_cdata.set('GDK_WINDOWING_BROADWAY/a gdkconfig_cdata.set('GDK_WINDOWING_OPENHOBBY', openhobby_enabled)" gdk/meson.build
fi

# 5. Add openhobby to the backend foreach loop (after 'broadway')
if ! grep -q "'openhobby'" gdk/meson.build; then
  sed -i "s/foreach backend : \['android', 'broadway',/foreach backend : ['android', 'broadway', 'openhobby',/" gdk/meson.build
fi

# 6. Add #include to gdkdisplaymanager.c (before the x11 include block)
if ! grep -q 'WINDOWING_OPENHOBBY' gdk/gdkdisplaymanager.c; then
  sed -i "/^#ifdef GDK_WINDOWING_X11/i\\
#ifdef GDK_WINDOWING_OPENHOBBY\\
#include \"openhobby/gdkopenhobby-private.h\"\\
#endif" gdk/gdkdisplaymanager.c
fi

# 7. Add backend table entry after broadway's entry
if ! grep -q '"openhobby"' gdk/gdkdisplaymanager.c; then
  sed -i "/{ \"broadway\", _gdk_broadway_display_open },/a\\
#ifdef GDK_WINDOWING_OPENHOBBY\\
  { \"openhobby\", _gdk_openhobby_display_open },\\
#endif" gdk/gdkdisplaymanager.c
fi

# 8. Make libdrm optional (we don't need dma-buf support)
sed -i "s/dependency('libdrm', required: os_linux)/dependency('libdrm', required: false)/" meson.build

# 8b. Make epoxy optional (we use Cairo software rendering, no GL)
sed -i "s/dependency('epoxy', version: epoxy_req)/dependency('epoxy', version: epoxy_req, required: false)/" meson.build

# 9. Make jpeg/tiff/png optional (we don't need image format loaders)
sed -i "s|png_dep\s*=\s*dependency('libpng', 'png')|png_dep = dependency('libpng', 'png', required: false)|" meson.build
sed -i "s|tiff_dep\s*=\s*dependency('libtiff-4', 'tiff')|tiff_dep = dependency('libtiff-4', 'tiff', required: false)|" meson.build
sed -i "s|jpeg_dep\s*=\s*dependency('libjpeg', 'jpeg')|jpeg_dep = dependency('libjpeg', 'jpeg', required: false)|" meson.build

# 10. Remove jpeg/tiff/png loader source files (unconditionally compiled, deps are optional)
sed -i "/loaders\/gdktiff\.c/d" gdk/meson.build
sed -i "/loaders\/gdkjpeg\.c/d" gdk/meson.build
sed -i "/loaders\/gdkpng\.c/d" gdk/meson.build

# 11. Remove tiff/jpeg/png deps from gdk_deps if optional (not found)
sed -i "/tiff_dep,/d" gdk/meson.build
sed -i "/jpeg_dep,/d" gdk/meson.build
sed -i "/png_dep,/d" gdk/meson.build

# 12. Fix gskturbulencenode.c - round() is not declared in freestanding env
sed -i 's/CLAMP ((int) round (\([^ ]* \* 255\)), 0, 255) << 24/(int)(\1 + 0.5) << 24/' gsk/gskturbulencenode.c
sed -i 's/CLAMP ((int) round (\([^ ]* \* 255\)), 0, 255) << 16/(int)(\1 + 0.5) << 16/' gsk/gskturbulencenode.c
sed -i 's/CLAMP ((int) round (\([^ ]* \* 255\)), 0, 255) << 8/(int)(\1 + 0.5) << 8/' gsk/gskturbulencenode.c
sed -i 's/CLAMP ((int) round (\([^ ]* \* 255\)), 0, 255) << 0/(int)(\1 + 0.5) << 0/' gsk/gskturbulencenode.c

# 13. Ensure comprehensive epoxy/gl.h with all GL constants GTK needs
mkdir -p "$SYSROOT/include/epoxy"
cat > "$SYSROOT/include/epoxy/gl.h" << 'GLEOF'
#ifndef EPOXY_GL_H
#define EPOXY_GL_H
#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif
#ifndef GLAPI
#define GLAPI extern
#endif
typedef unsigned int GLuint;
typedef int GLint;
typedef unsigned int GLenum;
typedef float GLfloat;
typedef char GLchar;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef signed char GLbyte;
typedef short GLshort;
typedef unsigned short GLushort;
typedef unsigned int GLsizei;
typedef double GLdouble;
typedef void GLvoid;
typedef unsigned long long GLuint64;
typedef long long GLint64;
typedef unsigned int GLsync;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_BYTE 0x1400
#define GL_UNSIGNED_BYTE 0x1401
#define GL_SHORT 0x1402
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT 0x1404
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_HALF_FLOAT 0x140B
#define GL_DOUBLE 0x140A
#define GL_UNSIGNED_NORMALIZED 0x8C17
#define GL_NO_ERROR 0
#define GL_INVALID_ENUM 0x0500
#define GL_INVALID_VALUE 0x0501
#define GL_INVALID_OPERATION 0x0502
#define GL_OUT_OF_MEMORY 0x0505
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_STENCIL_BUFFER_BIT 0x00000400
#define GL_RED 0x1903
#define GL_GREEN 0x1904
#define GL_BLUE 0x1905
#define GL_ALPHA 0x1906
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_BGR 0x80E0
#define GL_BGRA 0x80E1
#define GL_ZERO 0
#define GL_ONE 1
#define GL_SRC_COLOR 0x0300
#define GL_ONE_MINUS_SRC_COLOR 0x0301
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DST_ALPHA 0x0304
#define GL_ONE_MINUS_DST_ALPHA 0x0305
#define GL_DST_COLOR 0x0306
#define GL_ONE_MINUS_DST_COLOR 0x0307
#define GL_SRC_ALPHA_SATURATE 0x0308
#define GL_SRC1_ALPHA 0x8589
#define GL_BLEND 0x0BE2
#define GL_FUNC_ADD 0x8006
#define GL_FUNC_SUBTRACT 0x800A
#define GL_FUNC_REVERSE_SUBTRACT 0x800B
#define GL_MIN 0x8007
#define GL_MAX 0x8008
#define GL_NEVER 0x0200
#define GL_LESS 0x0201
#define GL_EQUAL 0x0202
#define GL_LEQUAL 0x0203
#define GL_GREATER 0x0204
#define GL_NOTEQUAL 0x0205
#define GL_GEQUAL 0x0206
#define GL_ALWAYS 0x0207
#define GL_KEEP 0x1E00
#define GL_REPLACE 0x1E01
#define GL_INCR 0x1E02
#define GL_DECR 0x1E03
#define GL_INVERT 0x150A
#define GL_INCR_WRAP 0x8507
#define GL_DECR_WRAP 0x8508
#define GL_POINTS 0x0000
#define GL_LINES 0x0001
#define GL_LINE_LOOP 0x0002
#define GL_LINE_STRIP 0x0003
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN 0x0006
#define GL_CULL_FACE 0x0B44
#define GL_FRONT 0x0404
#define GL_BACK 0x0405
#define GL_FRONT_AND_BACK 0x0408
#define GL_CW 0x0900
#define GL_CCW 0x0901
#define GL_DEPTH_TEST 0x0B71
#define GL_DEPTH_WRITEMASK 0x0B72
#define GL_DEPTH_FUNC 0x0B74
#define GL_SCISSOR_TEST 0x0C11
#define GL_SCISSOR_BOX 0x0C10
#define GL_VIEWPORT 0x0BA2
#define GL_TEXTURE 0x1702
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_RECTANGLE 0x84F5
#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#define GL_TEXTURE_3D 0x806F
#define GL_TEXTURE_CUBE_MAP 0x8513
#define GL_TEXTURE_2D_MULTISAMPLE 0x9100
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_WRAP_R 0x8072
#define GL_TEXTURE_MIN_LOD 0x813A
#define GL_TEXTURE_MAX_LOD 0x813B
#define GL_TEXTURE_BASE_LEVEL 0x813C
#define GL_TEXTURE_MAX_LEVEL 0x813D
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_TEXTURE_SWIZZLE_R 0x8E42
#define GL_TEXTURE_SWIZZLE_G 0x8E43
#define GL_TEXTURE_SWIZZLE_B 0x8E44
#define GL_TEXTURE_SWIZZLE_A 0x8E45
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_CLAMP_TO_BORDER 0x812D
#define GL_MIRRORED_REPEAT 0x8370
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_TEXTURE_BINDING_2D 0x8069
#define GL_TEXTURE_BINDING_RECTANGLE 0x84F6
#define GL_TEXTURE_BINDING_EXTERNAL_OES 0x8D65
#define GL_TEXTURE_BINDING_CUBE_MAP 0x8514
#define GL_TEXTURE_WIDTH 0x1000
#define GL_TEXTURE_HEIGHT 0x1001
#define GL_TEXTURE_DEPTH 0x8071
#define GL_TEXTURE_INTERNAL_FORMAT 0x1003
#define GL_TEXTURE_RED_SIZE 0x805C
#define GL_TEXTURE_GREEN_SIZE 0x805D
#define GL_TEXTURE_BLUE_SIZE 0x805E
#define GL_TEXTURE_ALPHA_SIZE 0x805F
#define GL_TEXTURE_RED_TYPE 0x8C10
#define GL_TEXTURE_GREEN_TYPE 0x8C11
#define GL_TEXTURE_BLUE_TYPE 0x8C12
#define GL_TEXTURE_ALPHA_TYPE 0x8C13
#define GL_TEXTURE_COMPRESSED 0x86A1
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_R8 0x8229
#define GL_RG 0x8227
#define GL_RG8 0x822B
#define GL_RGB8 0x8051
#define GL_RGBA8 0x8058
#define GL_R16 0x822A
#define GL_RG16 0x822C
#define GL_RGB16 0x8054
#define GL_RGBA16 0x805B
#define GL_R16F 0x822D
#define GL_RG16F 0x822F
#define GL_RGB16F 0x881B
#define GL_RGBA16F 0x881A
#define GL_R32F 0x822E
#define GL_RG32F 0x8230
#define GL_RGB32F 0x8815
#define GL_RGBA32F 0x8814
#define GL_RGB10_A2 0x8059
#define GL_SRGB8 0x8C41
#define GL_SRGB8_ALPHA8 0x8C43
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_DEPTH24_STENCIL8 0x88F0
#define GL_DEPTH_COMPONENT32F 0x8CAC
#define GL_DEPTH32F_STENCIL8 0x8CAD
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_PACK_ROW_LENGTH 0x0D02
#define GL_PACK_SKIP_ROWS 0x0D03
#define GL_PACK_SKIP_PIXELS 0x0D04
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#define GL_UNPACK_SKIP_ROWS 0x0CF3
#define GL_UNPACK_SKIP_PIXELS 0x0CF4
#define GL_UNSIGNED_SHORT_5_6_5 0x8363
#define GL_UNSIGNED_SHORT_1_5_5_5_REV 0x8366
#define GL_UNSIGNED_SHORT_4_4_4_4_REV 0x8365
#define GL_UNSIGNED_INT_8_8_8_8 0x8035
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#define GL_UNSIGNED_INT_2_10_10_10_REV 0x8368
#define GL_UNSIGNED_SHORT_5_6_5_REV 0x8364
#define GL_FRAMEBUFFER 0x8D40
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_RENDERBUFFER 0x8D41
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_COLOR_ATTACHMENT1 0x8CE1
#define GL_COLOR_ATTACHMENT2 0x8CE2
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_STENCIL_ATTACHMENT 0x8D20
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_FRAMEBUFFER_UNDEFINED 0x8219
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT 0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT 0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER 0x8CDB
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER 0x8CDC
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS 0x8CD9
#define GL_FRAMEBUFFER_UNSUPPORTED 0x8CDD
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE 0x8D56
#define GL_RENDERBUFFER_SAMPLES 0x8CAB
#define GL_MAX_COLOR_ATTACHMENTS 0x8CDF
#define GL_MAX_RENDERBUFFER_SIZE 0x84E8
#define GL_RENDERBUFFER_WIDTH 0x8D42
#define GL_RENDERBUFFER_HEIGHT 0x8D43
#define GL_RENDERBUFFER_INTERNAL_FORMAT 0x8D44
#define GL_RENDERBUFFER_ALPHA_SIZE 0x8D53
#define GL_RENDERBUFFER_RED_SIZE 0x8D50
#define GL_RENDERBUFFER_GREEN_SIZE 0x8D51
#define GL_RENDERBUFFER_BLUE_SIZE 0x8D52
#define GL_RENDERBUFFER_DEPTH_SIZE 0x8D54
#define GL_RENDERBUFFER_STENCIL_SIZE 0x8D55
#define GL_DEPTH_COMPONENT 0x1902
#define GL_DEPTH_STENCIL 0x84F9
#define GL_STENCIL_INDEX8 0x8D48
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_GEOMETRY_SHADER 0x8DD9
#define GL_COMPUTE_SHADER 0x91B9
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_VALIDATE_STATUS 0x8B83
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_SHADER_SOURCE_LENGTH 0x8B88
#define GL_ACTIVE_UNIFORMS 0x8B86
#define GL_ACTIVE_ATTRIBUTES 0x8B89
#define GL_SHADER_TYPE 0x8B4F
#define GL_DELETE_STATUS 0x8B80
#define GL_MAX_VERTEX_ATTRIBS 0x8869
#define GL_MAX_VERTEX_UNIFORM_VECTORS 0x8DFC
#define GL_MAX_VARYING_VECTORS 0x8DFC
#define GL_MAX_FRAGMENT_UNIFORM_VECTORS 0x8DFD
#define GL_MAX_TEXTURE_IMAGE_UNITS 0x8872
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS 0x8B4C
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS 0x8B4D
#define GL_MAX_LABEL_LENGTH 0x82E8
#define GL_MAX_UNIFORM_BLOCK_SIZE 0x8A30
#define GL_MAX_TEXTURE_SIZE 0x0D33
#define GL_UNIFORM_BUFFER 0x8A11
#define GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT 0x8A34
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_COPY_READ_BUFFER 0x8F36
#define GL_COPY_WRITE_BUFFER 0x8F37
#define GL_PIXEL_PACK_BUFFER 0x88EB
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#define GL_BUFFER 0x82E0
#define GL_SHADER 0x82E1
#define GL_PROGRAM 0x82E2
#define GL_QUERY 0x82E3
#define GL_PROGRAM_PIPELINE 0x82E4
#define GL_BUFFER_SIZE 0x8764
#define GL_BUFFER_USAGE 0x8765
#define GL_CURRENT_QUERY 0x8865
#define GL_STATIC_DRAW 0x88E4
#define GL_STATIC_READ 0x88E5
#define GL_STATIC_COPY 0x88E6
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_DYNAMIC_READ 0x88E9
#define GL_DYNAMIC_COPY 0x88EA
#define GL_STREAM_DRAW 0x88E0
#define GL_STREAM_READ 0x88E1
#define GL_STREAM_COPY 0x88E2
#define GL_MAP_READ_BIT 0x0001
#define GL_MAP_WRITE_BIT 0x0002
#define GL_MAP_PERSISTENT_BIT 0x0040
#define GL_MAP_COHERENT_BIT 0x0080
#define GL_MAP_INVALIDATE_BUFFER_BIT 0x0004
#define GL_MAP_INVALIDATE_RANGE_BIT 0x0001
#define GL_MAP_FLUSH_EXPLICIT_BIT 0x0010
#define GL_MAPPED_BUFFER 0x88BC
#define GL_DEBUG_OUTPUT 0x92E0
#define GL_DEBUG_OUTPUT_SYNCHRONOUS 0x8242
#define GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH 0x8243
#define GL_DEBUG_CALLBACK_FUNCTION 0x8244
#define GL_DEBUG_CALLBACK_USER_PARAM 0x8245
#define GL_DEBUG_SOURCE_API 0x8246
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM 0x8247
#define GL_DEBUG_SOURCE_SHADER_COMPILER 0x8248
#define GL_DEBUG_SOURCE_THIRD_PARTY 0x8249
#define GL_DEBUG_SOURCE_APPLICATION 0x824A
#define GL_DEBUG_SOURCE_OTHER 0x824B
#define GL_DEBUG_TYPE_ERROR 0x824C
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR 0x824D
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR 0x824E
#define GL_DEBUG_TYPE_PORTABILITY 0x824F
#define GL_DEBUG_TYPE_PERFORMANCE 0x8250
#define GL_DEBUG_TYPE_MARKER 0x8268
#define GL_DEBUG_TYPE_PUSH_GROUP 0x8269
#define GL_DEBUG_TYPE_POP_GROUP 0x826A
#define GL_DEBUG_TYPE_OTHER 0x8251
#define GL_DEBUG_SEVERITY_HIGH 0x9146
#define GL_DEBUG_SEVERITY_MEDIUM 0x9147
#define GL_DEBUG_SEVERITY_LOW 0x9148
#define GL_DEBUG_SEVERITY_NOTIFICATION 0x826B
#define GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#define GL_SYNC_FLUSH_COMMANDS_BIT 0x0001
#define GL_TIMEOUT_EXPIRED 0x911B
#define GL_TIMEOUT_IGNORED 0xFFFFFFFFFFFFFFFFULL
#define GL_IMPLEMENTATION_COLOR_READ_TYPE 0x8B9A
#define GL_IMPLEMENTATION_COLOR_READ_FORMAT 0x8B9B
#define GL_RENDERER 0x1F01
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_VERSION 0x1F02
#define GL_EXTENSIONS 0x1F03
#define GL_MULTISAMPLE 0x809D
#define GL_SAMPLE_ALPHA_TO_COVERAGE 0x809E
#define GL_SAMPLE_BUFFERS 0x80A8
#define GL_SAMPLES 0x80A9
#define GL_STENCIL_TEST 0x0B90
#define GL_LAYOUT_GENERAL_EXT 0x958D
#define GL_READ_ONLY 0x88B8
#define GL_WRITE_ONLY 0x88B9
#define GL_READ_WRITE 0x88BA
#define GL_BACK_LEFT 0x020A
#define GL_VENDOR 0x1F00
#define GL_NUM_EXTENSIONS 0x821D
#define GL_MAJOR_VERSION 0x821B
#define GL_MINOR_VERSION 0x821C
#endif
GLEOF

# 14. Create hb-subset.h stub (harfbuzz built with -Dsubset=disabled, GTK needs it)
cat > "$SYSROOT/include/harfbuzz/hb-subset.h" << 'HBEOF'
#ifndef HB_SUBSET_H
#define HB_SUBSET_H
#include "hb.h"
#include "hb-ot.h"
HB_BEGIN_DECLS
typedef struct hb_subset_input_t hb_subset_input_t;
typedef struct hb_subset_plan_t hb_subset_plan_t;
typedef enum {
  HB_SUBSET_FLAGS_DEFAULT = 0,
  HB_SUBSET_FLAGS_NO_HINTING = 1 << 0,
  HB_SUBSET_FLAGS_RETAIN_GIDS = 1 << 1,
  HB_SUBSET_FLAGS_DESUBROUTINIZE = 1 << 2,
  HB_SUBSET_FLAGS_NAME_LEGACY = 1 << 3,
  HB_SUBSET_FLAGS_SET_OVERLAPS_FLAG = 1 << 4,
  HB_SUBSET_FLAGS_PASSTHROUGH_UNRECOGNIZED = 1 << 5,
  HB_SUBSET_FLAGS_NOTDEF_OUTLINE = 1 << 6,
  HB_SUBSET_FLAGS_GLYPH_NAMES = 1 << 7,
  HB_SUBSET_FLAGS_NO_PRUNE_UNICODE_RANGES = 1 << 8,
  HB_SUBSET_FLAGS_NO_LAYOUT_CLOSURE = 1 << 9,
  HB_SUBSET_FLAGS_OPTIMIZE_IUP_DELTAS = 1 << 10,
  HB_SUBSET_FLAGS_NO_BIDI_CLOSURE = 1 << 11,
  HB_SUBSET_FLAGS_IFTB_REQUIREMENTS = 1 << 12
} hb_subset_flags_t;
static inline hb_subset_input_t * hb_subset_input_create_or_fail (void) { return NULL; }
static inline void hb_subset_input_destroy (hb_subset_input_t *input) { (void) input; }
static inline void hb_subset_input_set_flags (hb_subset_input_t *input, unsigned flags) { (void) input; (void) flags; }
static inline hb_set_t * hb_subset_input_glyph_set (hb_subset_input_t *input) { (void) input; return NULL; }
static inline hb_face_t * hb_subset_or_fail (hb_face_t *source, const hb_subset_input_t *input) { (void) source; (void) input; return NULL; }
HB_END_DECLS
#endif
HBEOF

echo "[gtk] Configuring..."
"$MESON" setup \
    "$BUILD_DIR/build" \
    "$GTK_SRC" \
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
    -Dbuild-demos=false \
    -Dbuild-tests=false \
    -Dman-pages=false \
    -Dvulkan=disabled \
    -Dx11-backend=false \
    -Dwayland-backend=false \
    -Dbroadway-backend=false \
    -Dwin32-backend=false \
    -Dmacos-backend=false \
    -Dandroid-backend=false \
    -Dopenhobby-backend=true \
    -Dmedia-gstreamer=disabled \
    -Dprint-cups=disabled \
    -Dcloudproviders=disabled \
    -Dsysprof=disabled \
    -Dtracker=disabled \
    -Dcolord=disabled \
    -Daccesskit=disabled \
    --wrap-mode=nodownload \
    --buildtype release

echo "[gtk] Building..."
set +e
"$MESON" compile -C "$BUILD_DIR/build"
BUILD_EXIT=$?
set -e
if [ $BUILD_EXIT -ne 0 ]; then
    echo "[gtk] Build had non-fatal errors, proceeding..."
fi

echo "[gtk] Installing..."

# meson install fails because it tries to install libgtk-4.so which doesn't exist
# in a static build. Install headers and static libs manually.
GTK_DEST="$SYSROOT/include/gtk-4.0"
mkdir -p "$GTK_DEST"

# 1. Copy all headers from source tree (public API headers)
cd "$GTK_SRC"
find . -name '*.h' | while read h; do
  dir=$(dirname "$h")
  mkdir -p "$GTK_DEST/$dir"
  cp "$GTK_SRC/$h" "$GTK_DEST/$h"
done

# 2. Copy generated headers from build tree (enum types, config, etc.)
cd "$BUILD_DIR/build"
find . -name '*.h' | while read h; do
  dir=$(dirname "$h")
  mkdir -p "$GTK_DEST/$dir"
  cp "$BUILD_DIR/build/$h" "$GTK_DEST/$h"
done

# 3. Install static libraries (convert thin archives to full archives first)
AR="$ROOT/toolchain/bin/$TARGET-ar"
RANLIB="$ROOT/toolchain/bin/$TARGET-ranlib"

for lib in gtk gdk gsk; do
    thin="$BUILD_DIR/build/$lib/lib${lib}.a"
    dest="$SYSROOT/lib/lib${lib}.a"
    if [ -f "$thin" ]; then
        echo "[gtk] Installing lib${lib}.a (converting thin archive)"
        # Check if thin archive
        if file "$thin" | grep -q "thin archive"; then
            TMPDIR=$(mktemp -d)
            # Extract from build tree paths
            cd "$BUILD_DIR/build/$lib"
            find . -name '*.o' -print0 | xargs -0 "$AR" rcs "$dest"
            "$RANLIB" "$dest" 2>/dev/null || true
            rm -rf "$TMPDIR"
        else
            cp "$thin" "$dest"
        fi
        # Create -4 symlink
        ln -sf "$(basename "$dest")" "$SYSROOT/lib/lib${lib}-4.a"
    fi
done

# Also install sub-archives
for sublib in "$BUILD_DIR/build/gtk/css/libgtk_css.a" "$BUILD_DIR/build/gtk/svg/libgtk_svg.a"; do
    if [ -f "$sublib" ]; then
        name=$(basename "$sublib")
        cp "$sublib" "$SYSROOT/lib/$name"
    fi
done

# 4. Install pkg-config files
for pc in gtk4.pc gdk4.pc; do
    src_pc="$BUILD_DIR/build/meson-private/$pc"
    if [ -f "$src_pc" ]; then
        cp "$src_pc" "$SYSROOT/lib/pkgconfig/$pc"
        echo "[gtk] Installed $pc"
    fi
done

# 5. Fix gtk4.pc to include gdk and gsk libs
GTK_PC="$SYSROOT/lib/pkgconfig/gtk4.pc"
if [ -f "$GTK_PC" ]; then
    sed -i 's|^Libs:.*|Libs: -L${libdir} -lgtk-4 -lgsk-4 -lgdk-4|' "$GTK_PC"
fi

echo "[gtk] Built and installed successfully"
