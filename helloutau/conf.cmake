include(GNUInstallDirs)

# ----------------------------------
# Project Constants
# ----------------------------------
set(HELLOUTAU_INCLUDE_DIR "include")

# The files for development, see HELLO_DEVEL, so that plugins can be built against the editor
# libraries and the plugins: <proj>_DEVEL gates the import libraries, the headers and the CMake
# package, <proj>_INSTALL_PDB the debug symbols.
set(HELLOUTAU_DEVEL ${HELLO_DEVEL})
set(HELLOUTAU_INSTALL_PDB ${HELLO_DEVEL})

set(HELLOUTAU_INSTALL_CONFIG_TEMPLATE
    "${CMAKE_CURRENT_LIST_DIR}/${HELLOUTAU_INSTALL_NAME}Config.cmake.in"
)

# The plugin layout of an application: <lib>/<name>/plugins, with the plugins of Qt in
# <lib>/Qt/plugins. A macOS bundle keeps that of qmsetup, Contents/Plugins beside Contents/MacOS
# of the program.
if(NOT(APPLE AND HELLOUTAU_MACOSX_BUNDLE_NAME))
    set(HELLOUTAU_BUILD_PLUGINS_DIR ${QMSETUP_BUILD_DIR}/lib/${HELLOUTAU_INSTALL_NAME}/plugins)
    set(HELLOUTAU_INSTALL_PLUGINS_DIR ${CMAKE_INSTALL_LIBDIR}/${HELLOUTAU_INSTALL_NAME}/plugins)
endif()

# The interface ID of the native plugins, the same as hello::daw::AppLoader::pluginIid, which
# test_AppLoader checks by loading a plugin built with this one.
set(HELLOUTAU_PLUGIN_IID "org.OpenVPI.HelloUtau.Plugin")

# Windows resource metadata. The libraries share the description of the module. A program is
# described by the DESCRIPTION of the project() of its own directory, which the function reads in
# the scope of that directory. project() without DESCRIPTION leaves PROJECT_DESCRIPTION empty.
set(HELLOUTAU_RC_DESCRIPTION "${PROJECT_DESCRIPTION}")
set(HELLOUTAU_RC_COPYRIGHT "Copyright (c) 2026-present SineStriker")

function(_helloutau_common_configure_target _target)
    set(_description "${HELLOUTAU_RC_DESCRIPTION}")

    if(PROJECT_DESCRIPTION)
        set(_description "${PROJECT_DESCRIPTION}")
    endif()

    # An application writes its own resource with its icon, see tools/driver/CMakeLists.txt.
    get_target_property(_type ${_target} HELLOUTAU_TARGET_TYPE)

    if(WIN32 AND NOT _type STREQUAL "Application")
        qm_add_win_rc(${_target}
            NAME ${_target}
            DESCRIPTION "${_description}"
            COPYRIGHT "${HELLOUTAU_RC_COPYRIGHT}"
        )
    endif()

    helloutau_set_default_install_rpath(${_target})
endfunction()

set(HELLOUTAU_POST_CONFIGURE_COMMANDS _helloutau_common_configure_target)

# Emit the definitions of qm_add_definition() in helloutau/CMakeLists.txt.
set(HELLOUTAU_CONFIG_HEADER_PATH ${HELLOUTAU_INSTALL_NAME}/Config.h)

# ----------------------------------
# Include Build Helpers
# ----------------------------------
qm_import(private/BuildSystem)

# Named for the module rather than for PROJECT_NAME, which each sub-library sets to its own target
# name.
qm_setup_build_repo_helpers(helloutau)
