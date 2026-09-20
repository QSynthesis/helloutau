# The external packages this repository builds against, each pointed at by its own `<name>_DIR`.
#
#     -Dstdcorelib_DIR=<prefix>/lib/cmake/stdcorelib
#     -Dstdutau_DIR=<prefix>/lib/cmake/stdutau
#     -Dwolf-midi_DIR=<vcpkg>/installed/<triplet>/share/wolf-midi
#
# stdcorelib and stdutau are moving alongside this repository, so neither is taken from a vcpkg
# release. Build and install each one yourself.
#
# Included from the root rather than added as a subdirectory, so that the imported targets are in
# scope for every module.

# The DLL is copied into the runtime output directory because vcpkg's applocal deployment does not
# cover a package found this way, and both the application and the test executables land there.
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

# The MIDI file reader and writer. It is QMidiFile with Qt taken out of it, so it speaks
# std::filesystem and std::vector and needs nothing from this side.
_hello_find_external(wolf-midi wolf-midi::wolf-midi
    "Install the wolf-midi vcpkg port and pass -Dwolf-midi_DIR=<vcpkg>/installed/<triplet>/share/wolf-midi.")
