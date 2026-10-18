# The configurations of the build. See the workflow in docs/Distribution.md.
#
# Release, the configuration of the published builds, carries the debug information, as the
# Release builds of vcpkg do, so that the symbols of the program and of its dependencies serve
# the analysis of crash dumps. The options of the compiler follow its front end: the MSVC front
# end embeds the debug information in the objects (/Z7), as vcpkg does, which a compiler cache can
# store, and the other front ends take -g.
if(MSVC)
    foreach(_lang C CXX)
        string(APPEND CMAKE_${_lang}_FLAGS_RELEASE " /Z7")
    endforeach()
else()
    foreach(_lang C CXX)
        string(APPEND CMAKE_${_lang}_FLAGS_RELEASE " -g")
    endforeach()
endif()
unset(_lang)

# The options of the linker follow the ABI, as QMSETUP_MSVC_ABI of qmsetup, which is set only
# after this file: clang with the GNU front end targeting the MSVC ABI links with link or lld-link
# as cl does. /DEBUG writes the PDB and keeps the unreferenced and the identical functions unless
# /OPT:REF and /OPT:ICF are given again. The GNU front end takes them through -Xlinker.
if(MSVC OR CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    set(_options /DEBUG /OPT:REF /OPT:ICF)
    if(NOT MSVC)
        list(TRANSFORM _options PREPEND "-Xlinker ")
    endif()
    list(JOIN _options " " _options)
    foreach(_kind EXE MODULE SHARED)
        string(APPEND CMAKE_${_kind}_LINKER_FLAGS_RELEASE " ${_options}")
    endforeach()
    unset(_options)
    unset(_kind)
endif()

# The packages are installed in the configurations Release and Debug, because vcpkg builds no
# other configuration. In a local build as RelWithDebInfo, an imported target without that
# configuration takes the first configuration that it lists unless a mapping is given, which can
# be Debug, whose runtime does not match.
if(NOT DEFINED CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO)
    set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO RelWithDebInfo Release)
endif()
