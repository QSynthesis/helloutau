include(GNUInstallDirs)

# ----------------------------------
# Project Constants
# ----------------------------------
set(HELLOUTAU_INCLUDE_DIR "include")

# The editor libraries install their headers, so that plugins can be built against them.
set(HELLOUTAU_DEVEL ON)

set(HELLOUTAU_INSTALL_CONFIG_TEMPLATE
    "${CMAKE_CURRENT_LIST_DIR}/${HELLOUTAU_INSTALL_NAME}Config.cmake.in"
)

# Keep the plugin layout: <lib>/plugins/<name>.
set(HELLOUTAU_BUILD_PLUGINS_DIR ${QMSETUP_BUILD_DIR}/lib/plugins/helloutau)
set(HELLOUTAU_INSTALL_PLUGINS_DIR ${CMAKE_INSTALL_LIBDIR}/plugins/helloutau)

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

    if(WIN32)
        qm_add_win_rc(${_target}
            NAME ${_target}
            DESCRIPTION "${_description}"
            COPYRIGHT "${HELLOUTAU_RC_COPYRIGHT}"
        )
    endif()

    helloutau_set_default_install_rpath(${_target})
endfunction()

set(HELLOUTAU_POST_CONFIGURE_COMMANDS _helloutau_common_configure_target)

# ----------------------------------
# Include Build Helpers
# ----------------------------------
qm_import(private/BuildSystem)

# Named for the module rather than for PROJECT_NAME, which each sub-library sets to its own target
# name, HelloUtauWidgets and the rest.
qm_setup_build_repo_helpers(helloutau)
