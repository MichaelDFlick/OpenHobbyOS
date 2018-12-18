# Create stub imported targets for QML tools that are needed at configure time
# but which we don't build for cross-compilation.
# These satisfy generator expressions like $<TARGET_FILE:Qt6::qmltyperegistrar>
# so the build configuration can proceed, but the actual tool functionality is skipped.

# Find the qml_stub_tool.sh relative to this file
get_filename_component(_QML_STUB_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
set(_QML_STUB_TOOL "${_QML_STUB_DIR}/qml_stub_tool.sh")

function(_qtdeclarative_create_tool_stub name)
    if(NOT TARGET Qt6::${name})
        add_executable(Qt6::${name} IMPORTED)
        set_target_properties(Qt6::${name} PROPERTIES
            IMPORTED_LOCATION "${_QML_STUB_TOOL}"
            IMPORTED_LOCATION_RELEASE "${_QML_STUB_TOOL}"
        )
        message(STATUS "Created stub target Qt6::${name} (using qml_stub_tool.sh)")
    endif()
endfunction()

_qtdeclarative_create_tool_stub(qmltyperegistrar)
_qtdeclarative_create_tool_stub(qmllint)
_qtdeclarative_create_tool_stub(qmlcachegen)
_qtdeclarative_create_tool_stub(qmlaotstats)
_qtdeclarative_create_tool_stub(qmlprofiler)
_qtdeclarative_create_tool_stub(qmltc)
_qtdeclarative_create_tool_stub(qmlcontextpropertydump)
_qtdeclarative_create_tool_stub(qmldom)
_qtdeclarative_create_tool_stub(qmljsrootgen)
_qtdeclarative_create_tool_stub(qmlformat)
_qtdeclarative_create_tool_stub(qmlimportscanner)
_qtdeclarative_create_tool_stub(qmlcompiler)
_qtdeclarative_create_tool_stub(qmlls)
_qtdeclarative_create_tool_stub(qmltime)
_qtdeclarative_create_tool_stub(qmlplugindump)
_qtdeclarative_create_tool_stub(qmltestrunner)

# Include real shader tools macros from qtshadertools (provides qt_internal_add_shaders)
if(EXISTS "${QT_HOST_PATH}/lib/cmake/Qt6ShaderToolsTools/Qt6ShaderToolsMacros.cmake")
    include("${QT_HOST_PATH}/lib/cmake/Qt6ShaderToolsTools/Qt6ShaderToolsMacros.cmake")
    message(STATUS "Loaded Qt6ShaderToolsMacros from ${QT_HOST_PATH}")
elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../user/lib/graphics32/qtshadertools/tools/qsb/Qt6ShaderToolsMacros.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/../../user/lib/graphics32/qtshadertools/tools/qsb/Qt6ShaderToolsMacros.cmake")
    message(STATUS "Loaded Qt6ShaderToolsMacros from source tree")
endif()
