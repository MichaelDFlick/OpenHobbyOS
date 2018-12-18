get_filename_component(OPENHOBBYOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(OPENHOBBYOS_USER_LD "${OPENHOBBYOS_ROOT}/user.ld" CACHE FILEPATH "OpenHobbyOS user linker script")

set(OPENHOBBYOS_TARGET "i686-openhobbyos" CACHE STRING "OpenHobbyOS cross target")
set(OPENHOBBYOS_SYSROOT "${OPENHOBBYOS_ROOT}/build/ports/sysroot" CACHE PATH "OpenHobbyOS sysroot")

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR i686)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_SYSROOT "${OPENHOBBYOS_SYSROOT}")

set(CMAKE_C_COMPILER "${OPENHOBBYOS_TARGET}-gcc")
set(CMAKE_CXX_COMPILER "${OPENHOBBYOS_TARGET}-g++")

set(CMAKE_AR "${OPENHOBBYOS_TARGET}-ar")
set(CMAKE_RANLIB "${OPENHOBBYOS_TARGET}-ranlib")
set(CMAKE_STRIP "${OPENHOBBYOS_TARGET}-strip")
set(PKG_CONFIG_EXECUTABLE "${OPENHOBBYOS_TARGET}-pkg-config")

set(CMAKE_FIND_ROOT_PATH "${OPENHOBBYOS_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_C_FLAGS_INIT "-O2 -D_GNU_SOURCE -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700 -D_POSIX_C_SOURCE=200809L -D_POSIX_THREADS -DFORKFD_NO_FORKFD=1 -D_GLIBCXX_HOSTED=0 -Wno-char-subscripts")
set(CMAKE_CXX_FLAGS_INIT "-O2 -D_GNU_SOURCE -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700 -D_POSIX_C_SOURCE=200809L -D_POSIX_THREADS -DFORKFD_NO_FORKFD=1 -D_GLIBCXX_HOSTED=0 -fno-freestanding -Wno-char-subscripts")
set(CMAKE_EXE_LINKER_FLAGS_INIT "--sysroot=${OPENHOBBYOS_SYSROOT} -static -Wl,-T,${OPENHOBBYOS_USER_LD} -nostartfiles -L${OPENHOBBYOS_SYSROOT}/lib -lopenhobbyosgloss")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "--sysroot=${OPENHOBBYOS_SYSROOT} -nostartfiles -Wl,-T,${OPENHOBBYOS_USER_LD} -L${OPENHOBBYOS_SYSROOT}/lib -Wl,--start-group -lopenhobbyosgloss -lstdc++ -Wl,--end-group -lgcc_eh")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "--sysroot=${OPENHOBBYOS_SYSROOT} -nostartfiles -Wl,-T,${OPENHOBBYOS_USER_LD} -L${OPENHOBBYOS_SYSROOT}/lib -Wl,--start-group -lopenhobbyosgloss -lstdc++ -Wl,--end-group -lgcc_eh")

set(OPENHOBBYOS ON CACHE BOOL "Targeting OpenHobbyOS")

set(QT_BUILD_EXAMPLES OFF CACHE BOOL "Disable examples")
set(QT_BUILD_TESTS OFF CACHE BOOL "Disable tests")
set(QT_BUILD_TOOLS_WHEN_CROSSCOMPILING ON CACHE BOOL "Build tools for cross-compilation")

set(FEATURE_linuxfb OFF CACHE BOOL "Disable linuxfb")
set(FEATURE_xcb OFF CACHE BOOL "Disable xcb")
set(FEATURE_wayland OFF CACHE BOOL "Disable wayland")
set(FEATURE_eglfs OFF CACHE BOOL "Disable eglfs")
set(FEATURE_directfb OFF CACHE BOOL "Disable directfb")
set(FEATURE_opengl OFF CACHE BOOL "Disable OpenGL")
set(FEATURE_opengles2 OFF CACHE BOOL "Disable OpenGL ES 2")
set(FEATURE_vulkan OFF CACHE BOOL "Disable Vulkan")
set(FEATURE_system_pixman ON CACHE BOOL "Use system pixman")
set(FEATURE_fontconfig OFF CACHE BOOL "Disable fontconfig")
set(FEATURE_dbus OFF CACHE BOOL "Disable D-Bus")
set(FEATURE_slog2 OFF CACHE BOOL "Disable slog2")
set(FEATURE_journald OFF CACHE BOOL "Disable journald")

# Link libopenhobbyosgloss after object files so its symbols
# can satisfy references from implicitly-linked libstdc++.a.
set(CMAKE_CXX_STANDARD_LIBRARIES "-lopenhobbyosgloss" CACHE STRING "C++ standard libraries")

set(PIXMAN_INCLUDE_DIR "${OPENHOBBYOS_SYSROOT}/include/pixman-1" CACHE PATH "Pixman include directory")
set(PIXMAN_LIBRARY "${OPENHOBBYOS_SYSROOT}/lib/libpixman-1.a" CACHE FILEPATH "Pixman library")
