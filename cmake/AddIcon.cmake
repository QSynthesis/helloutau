# Icons written at build time by qiconmaker (https://github.com/stdware/qiconmaker) from drawings,
# so that the repository keeps the drawings rather than the icon files. The package qiconmaker is
# found in third-party/Dependencies.cmake. See the icons in docs/Distribution.md.

# Qt Core for the directory of its DLLs, which the program loads on Windows
qm_find_qt(Core)

# add_icon(<output> <master>
#     [SMALL_DRAWINGS <prefix>]
# )
#
# Writes the icon \a output, an .ico of Windows or an .icns of macOS by its suffix, from the
# master drawing \a master, an SVG or a PNG, in a custom command. A target of the same directory
# that lists \a output as a source builds the icon first, and qm_add_win_rc() and
# qm_add_mac_bundle() of qmsetup accept it.
#
# \c SMALL_DRAWINGS names the drawings aligned to the pixel grids of the small sizes,
# <prefix>-<size>.svg, as far as they exist. The drawings of 16, 20, 24, 32 and 48 px serve their
# sizes, the drawing of 24 px also 30 px, and the drawing of 48 px also 36 px and 40 px, because
# Windows uses these sizes at the scales between. An .icns holds 16 px and 32 px of them. Every
# other size is rendered from the master drawing.
function(add_icon _output _master)
    set(options)
    set(oneValueArgs SMALL_DRAWINGS)
    set(multiValueArgs)
    cmake_parse_arguments(FUNC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(_arguments "${_output}" "${_master}")
    set(_depends "${_master}")
    if(FUNC_SMALL_DRAWINGS)
        set(_assignments 16:16 20:20 24:24 30:24 32:32 36:48 40:48 48:48)
        if(_output MATCHES "\\.icns$")
            set(_assignments 16:16 32:32)
        endif()
        foreach(_assignment IN LISTS _assignments)
            string(REPLACE ":" ";" _assignment "${_assignment}")
            list(GET _assignment 0 _size)
            list(GET _assignment 1 _drawing)
            set(_file "${FUNC_SMALL_DRAWINGS}-${_drawing}.svg")
            if(EXISTS "${_file}")
                list(APPEND _arguments "${_size}=${_file}")
                list(APPEND _depends "${_file}")
            endif()
        endforeach()
        list(REMOVE_DUPLICATES _depends)
    endif()

    # The program loads the DLLs of Qt, which Windows finds through PATH. On the other systems the
    # program holds the run path to the libraries of Qt.
    set(_command $<TARGET_FILE:qiconmaker::qiconmaker>)
    if(WIN32)
        set(_command ${CMAKE_COMMAND} -E env "PATH=$<TARGET_FILE_DIR:Qt6::Core>" ${_command})
    endif()
    add_custom_command(OUTPUT "${_output}"
        COMMAND ${_command} ${_arguments}
        DEPENDS ${_depends}
        COMMENT "Writing ${_output}"
        VERBATIM
    )
endfunction()
