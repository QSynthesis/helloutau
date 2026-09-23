# The external packages this repository builds against, each pointed at by its own `<name>_DIR`.
#
#     -Dstdcorelib_DIR=<prefix>/lib/cmake/stdcorelib
#     -Dstdutau_DIR=<prefix>/lib/cmake/stdutau
#     -Dwolf-midi_DIR=<prefix>/lib/cmake/wolf-midi
#     -Dwinacp_DIR=<prefix>/lib/cmake/winacp
#
# stdcorelib and stdutau are developed alongside this repository, so neither is taken from a vcpkg
# release. Each must be built and installed separately.
#
# Included from the root rather than added as a subdirectory, so that the imported targets are in
# scope for every module.

# The DLL is copied into the runtime output directory, because the applocal deployment of vcpkg
# does not cover packages found this way, and both the application and the tests run from there.
function(_hello_find_external _package _target _hint)
    if(NOT ${_package}_DIR)
        message(FATAL_ERROR "${_package}_DIR is not set. ${_hint}")
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

_hello_find_external(stdcorelib stdcorelib::stdcorelib
    "Build https://github.com/stdware/stdcorelib and pass -Dstdcorelib_DIR=<prefix>/lib/cmake/stdcorelib.")

_hello_find_external(stdutau stdutau::stdutau
    "Build https://github.com/diffscope/stdutau and pass -Dstdutau_DIR=<prefix>/lib/cmake/stdutau.")

# The MIDI file reader and writer, a version of QMidiFile without Qt. Its interface uses
# std::filesystem and std::vector and has no dependencies on this repository.
_hello_find_external(wolf-midi wolf-midi::wolf-midi
    "Build https://github.com/wolfgitpr/wolf-midi and pass -Dwolf-midi_DIR=<prefix>/lib/cmake/wolf-midi.")

# Conversion of the Windows ANSI code pages, identical to that of Windows on every system, which a
# UST or an oto.ini written by UTAU requires to round-trip unchanged. See TextCodec.cpp for the
# reason Qt and ICU are not used.
_hello_find_external(winacp winacp::winacp
    "Build https://github.com/QSynthesis/winacp and pass -Dwinacp_DIR=<prefix>/lib/cmake/winacp.")
