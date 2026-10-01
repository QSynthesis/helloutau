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

# Keep the plugin layout: <lib>/plugins/<name>. A macOS bundle keeps that of qmsetup,
# Contents/Plugins beside Contents/MacOS of the program.
if(NOT(APPLE AND HELLOUTAU_MACOSX_BUNDLE_NAME))
    set(HELLOUTAU_BUILD_PLUGINS_DIR ${QMSETUP_BUILD_DIR}/lib/plugins/helloutau)
    set(HELLOUTAU_INSTALL_PLUGINS_DIR ${CMAKE_INSTALL_LIBDIR}/plugins/helloutau)
endif()

# The interface ID of the native plugins, the same as hello::daw::AppLoader::pluginIid, which
# test_AppLoader checks by loading a plugin built with this one.
set(HELLOUTAU_PLUGIN_IID "org.OpenVPI.HelloUtau.Plugin")

# Windows resource metadata. The libraries share the description of the module. A program is
# described by the DESCRIPTION of the project() of its own directory, which the function reads in
# the scope of that directory. project() without DESCRIPTION leaves PROJECT_DESCRIPTION empty.
# The name is that of the target unless the directory sets HELLOUTAU_RC_NAME, as a program whose
# file name is in lower case does for the name shown to the user.
set(HELLOUTAU_RC_DESCRIPTION "${PROJECT_DESCRIPTION}")
set(HELLOUTAU_RC_COPYRIGHT "Copyright (c) 2026-present SineStriker")
set(HELLOUTAU_RC_ICON "${CMAKE_CURRENT_LIST_DIR}/tools/driver/helloutau.ico")

function(_helloutau_common_configure_target _target)
    set(_description "${HELLOUTAU_RC_DESCRIPTION}")

    if(PROJECT_DESCRIPTION)
        set(_description "${PROJECT_DESCRIPTION}")
    endif()

    set(_name ${_target})

    if(HELLOUTAU_RC_NAME)
        set(_name "${HELLOUTAU_RC_NAME}")
    endif()

    if(WIN32 AND _target STREQUAL "${HELLOUTAU_INSTALL_NAME}")
        qm_add_win_rc(${_target}
            NAME "${_name}"
            DESCRIPTION "${_description}"
            COPYRIGHT "${HELLOUTAU_RC_COPYRIGHT}"
            ICON "${HELLOUTAU_RC_ICON}"
        )
    elseif(WIN32)
        qm_add_win_rc(${_target}
            NAME "${_name}"
            DESCRIPTION "${_description}"
            COPYRIGHT "${HELLOUTAU_RC_COPYRIGHT}"
        )
    endif()

    helloutau_set_default_install_rpath(${_target})
endfunction()

set(HELLOUTAU_POST_CONFIGURE_COMMANDS _helloutau_common_configure_target)

# Emit build metadata for the About dialog.
set(HELLOUTAU_BUILD_INFO_HEADER_PATH helloutau/BuildInfo.h)
set(HELLOUTAU_BUILD_INFO_HEADER_PREFIX HELLOUTAU)

# ----------------------------------
# Include Build Helpers
# ----------------------------------
qm_import(private/BuildSystem)

# Named for the module rather than for PROJECT_NAME, which each sub-library sets to its own target
# name, HelloUtauEditor and the rest.
qm_setup_build_repo_helpers(helloutau)
