# stdcorelib and stdutau are both moving alongside this repository, so neither is taken from vcpkg.
# Build and install each one yourself, then pass its package directory at configure time:
#
#     -Dstdcorelib_DIR=<prefix>/lib/cmake/stdcorelib
#     -Dstdutau_DIR=<prefix>/lib/cmake/stdutau
#
# Included from the root rather than added as a subdirectory, so that the imported targets are in
# scope for every module.

# The DLL is copied into the runtime output directory because vcpkg's applocal deployment no longer
# covers these two, and both the application and the test executables land there.
function(_hello_find_external _package _target _url)
    if(NOT ${_package}_DIR)
        message(FATAL_ERROR
            "${_package}_DIR is not set. Build ${_url} and pass "
            "-D${_package}_DIR=<prefix>/lib/cmake/${_package}.")
    endif()

    find_package(${_package} CONFIG REQUIRED)

    get_target_property(_type ${_target} TYPE)

    if(WIN32 AND _type STREQUAL "SHARED_LIBRARY")
        set(_deploy hello_deploy_${_package})
        add_custom_target(${_deploy} ALL)
        qm_add_copy_command(${_deploy}
            SOURCES $<TARGET_FILE:${_target}>
            DESTINATION bin
            SKIP_INSTALL
        )
    endif()
endfunction()

_hello_find_external(stdcorelib stdcorelib::stdcorelib https://github.com/stdware/stdcorelib)
_hello_find_external(stdutau stdutau::stdutau https://github.com/diffscope/stdutau)
