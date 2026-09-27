# The external packages this repository builds against, each pointed at by its own `<name>_DIR`.
#
#     -Dstdcorelib_DIR=<prefix>/lib/cmake/stdcorelib
#     -Dstdutau_DIR=<prefix>/lib/cmake/stdutau
#     -Dwolf-midi_DIR=<prefix>/lib/cmake/wolf-midi
#     -Dwinacp_DIR=<prefix>/lib/cmake/winacp
#     -Dsubstate_DIR=<prefix>/lib/cmake/substate
#     -DQActionKit_DIR=<prefix>/lib/cmake/QActionKit
#
# stdcorelib, stdutau, substate and QActionKit are developed alongside this repository, so none is
# taken from a vcpkg release. Each must be built and installed separately.
#
# Included from the root rather than added as a subdirectory, so that the imported targets are in
# scope for every module.

# Finds _package with the components given after COMPONENTS, last, and deploys its shared
# libraries, the targets given after _hint. The DLLs are copied into the runtime output directory,
# because the applocal deployment of vcpkg does not cover packages found this way, and both the
# application and the tests run from there.
function(_hello_find_external _package _hint)
    cmake_parse_arguments(FUNC "" "" "COMPONENTS" ${ARGN})
    if(NOT ${_package}_DIR)
        message(FATAL_ERROR "${_package}_DIR is not set. ${_hint}")
    endif()

    find_package(${_package} CONFIG REQUIRED COMPONENTS ${FUNC_COMPONENTS})

    set(_files)
    foreach(_target IN LISTS FUNC_UNPARSED_ARGUMENTS)
        get_target_property(_type ${_target} TYPE)
        if(_type STREQUAL "SHARED_LIBRARY")
            list(APPEND _files $<TARGET_FILE:${_target}>)
        endif()
    endforeach()

    if(WIN32 AND _files)
        set(_deploy hello_deploy_${_package})
        add_custom_target(${_deploy} ALL)
        qm_add_copy_command(${_deploy}
            SOURCES ${_files}
            DESTINATION bin
            SKIP_INSTALL
        )
    endif()
endfunction()

_hello_find_external(stdcorelib
    "Build https://github.com/stdware/stdcorelib and pass -Dstdcorelib_DIR=<prefix>/lib/cmake/stdcorelib."
    stdcorelib::stdcorelib)

_hello_find_external(stdutau
    "Build https://github.com/diffscope/stdutau and pass -Dstdutau_DIR=<prefix>/lib/cmake/stdutau."
    stdutau::stdutau)

# The MIDI file reader and writer, a version of QMidiFile without Qt. Its interface uses
# std::filesystem and std::vector and has no dependencies on this repository.
_hello_find_external(wolf-midi
    "Build https://github.com/wolfgitpr/wolf-midi and pass -Dwolf-midi_DIR=<prefix>/lib/cmake/wolf-midi."
    wolf-midi::wolf-midi)

# Conversion of the Windows ANSI code pages, identical to that of Windows on every system, which a
# UST or an oto.ini written by UTAU requires to round-trip unchanged. See TextCodec.cpp for the
# reason Qt and ICU are not used.
_hello_find_external(winacp
    "Build https://github.com/QSynthesis/winacp and pass -Dwinacp_DIR=<prefix>/lib/cmake/winacp."
    winacp::winacp)

# The document model with transactions and undo history, a private dependency of HelloKitEdit. The
# package provides two libraries: substate, the core without Qt, and qsubstate, the node types that
# hold QVariant values. See docs/Editing.md.
_hello_find_external(substate
    "Build https://github.com/stdware/substate and pass -Dsubstate_DIR=<prefix>/lib/cmake/substate."
    substate::substate substate::qsubstate)

# The menus, tool bars and shortcuts of the application, declared in action extension manifests
# that AEC compiles at build time. Only the Core and Widgets modules are used, and the component
# makes the package find Qt Widgets, which its Widgets module links.
_hello_find_external(QActionKit
    "Build https://github.com/stdware/qactionkit and pass -DQActionKit_DIR=<prefix>/lib/cmake/QActionKit."
    QActionKit::Core QActionKit::Widgets
    COMPONENTS Widgets)
