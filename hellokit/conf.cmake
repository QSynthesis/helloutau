include(GNUInstallDirs)

# ----------------------------------
# Project Constants
# ----------------------------------
set(HELLOKIT_INCLUDE_DIR "include")

# Install the CMake package files and the public headers alongside the binaries.
# The generic helpers gate both on <proj>_DEVEL, which defaults to off.
set(HELLOKIT_DEVEL ON)

set(HELLOKIT_INSTALL_CONFIG_TEMPLATE
    "${CMAKE_CURRENT_LIST_DIR}/${HELLOKIT_INSTALL_NAME}Config.cmake.in"
)

# Windows resource metadata. The libraries share the description of the module. A program is
# described by the DESCRIPTION of the project() of its own directory, which the function reads in
# the scope of that directory. project() without DESCRIPTION leaves PROJECT_DESCRIPTION empty.
set(HELLOKIT_RC_DESCRIPTION "${PROJECT_DESCRIPTION}")
set(HELLOKIT_RC_COPYRIGHT "Copyright (c) 2026-present SineStriker")

function(_hellokit_common_configure_target _target)
    set(_description "${HELLOKIT_RC_DESCRIPTION}")
    if(PROJECT_DESCRIPTION)
        set(_description "${PROJECT_DESCRIPTION}")
    endif()

    if(WIN32)
        qm_add_win_rc(${_target}
            NAME ${_target}
            DESCRIPTION "${_description}"
            COPYRIGHT "${HELLOKIT_RC_COPYRIGHT}"
        )
    endif()

    hellokit_set_default_install_rpath(${_target})
endfunction()

set(HELLOKIT_POST_CONFIGURE_COMMANDS _hellokit_common_configure_target)

# ----------------------------------
# Include Build Helpers
# ----------------------------------
qm_import(private/BuildSystem)

# Named for the module rather than for PROJECT_NAME, which each sub-library sets to its own target
# name, HelloKitUst and the rest.
qm_setup_build_repo_helpers(hellokit)
