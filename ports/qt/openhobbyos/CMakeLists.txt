# Copyright (C) 2025 The OpenHobbyOS Project
# SPDX-License-Identifier: BSD-3-Clause

#####################################################################
## QOpenHobbyOSIntegrationPlugin Plugin:
#####################################################################

qt_internal_add_plugin(QOpenHobbyOSIntegrationPlugin
    OUTPUT_NAME qopenhobbyos
    PLUGIN_TYPE platforms
    DEFAULT_IF "openhobbyos" IN_LIST QT_QPA_PLATFORMS
    SOURCES
        main.cpp
        qopenhobbyosbackingstore.cpp qopenhobbyosbackingstore.h
        qopenhobbyosclipboard.cpp qopenhobbyosclipboard.h
        qopenhobbyoscursor.cpp qopenhobbyoscursor.h
        qopenhobbyoseventdispatcher.cpp qopenhobbyoseventdispatcher.h
        qopenhobbyosfontdatabase.cpp qopenhobbyosfontdatabase.h
        qopenhobbyosinputcontext.cpp qopenhobbyosinputcontext.h
        qopenhobbyosintegration.cpp qopenhobbyosintegration.h
        qopenhobbyosnativeinterface.cpp qopenhobbyosnativeinterface.h
        qopenhobbyosscreen.cpp qopenhobbyosscreen.h
        qopenhobbyosservices.cpp qopenhobbyosservices.h
        qopenhobbyoswindow.cpp qopenhobbyoswindow.h
        # QRasterBackingStore is not included in QtGui for non-Apple/Android/OHOS/QNX
        ${CMAKE_SOURCE_DIR}/src/gui/painting/qrasterbackingstore.cpp
    DEFINES
        QT_NO_FOREACH
    LIBRARIES
        Qt::Core
        Qt::CorePrivate
        Qt::Gui
        Qt::GuiPrivate
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Find pixman via pkg-config or link directly
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PIXMAN pixman-1)
endif()
if(PIXMAN_FOUND)
    target_include_directories(QOpenHobbyOSIntegrationPlugin PRIVATE ${PIXMAN_INCLUDE_DIRS})
    target_link_directories(QOpenHobbyOSIntegrationPlugin PRIVATE ${PIXMAN_LIBRARY_DIRS})
    target_link_libraries(QOpenHobbyOSIntegrationPlugin PRIVATE ${PIXMAN_LIBRARIES})
else()
    target_link_libraries(QOpenHobbyOSIntegrationPlugin PRIVATE pixman-1)
    target_include_directories(QOpenHobbyOSIntegrationPlugin PRIVATE
        ${CMAKE_SOURCE_DIR}/../../../../build/ports/sysroot/include/pixman-1)
endif()

# Find XNX client library (pkg-config or direct)
find_library(XNX_LIBRARY xnx PATHS ${CMAKE_SOURCE_DIR}/../../../../build/ports/sysroot/lib NO_DEFAULT_PATH)
if(XNX_LIBRARY)
    target_link_libraries(QOpenHobbyOSIntegrationPlugin PRIVATE ${XNX_LIBRARY})
else()
    target_link_libraries(QOpenHobbyOSIntegrationPlugin PRIVATE xnx)
endif()
target_include_directories(QOpenHobbyOSIntegrationPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/../../../../build/ports/sysroot/include/xnx
    ${CMAKE_SOURCE_DIR}/../../xnx
    ${CMAKE_SOURCE_DIR}/../../../../user/xnx)
