# The configurations of the build. See the workflow in docs/Distribution.md.
#
# Release, the configuration of the published builds, carries the debug information, as the
# Release builds of vcpkg do, so that the symbols of the program and of its dependencies serve
# the analysis of crash dumps. The MSVC front end embeds the debug information in the objects
# (/Z7), as vcpkg does, which a compiler cache can store, and the linker writes a PDB for each
# binary. /DEBUG keeps the unreferenced and the identical functions unless /OPT:REF and /OPT:ICF
# are given again. The other compilers take -g.
if(MSVC)
    foreach(_lang C CXX)
        string(APPEND CMAKE_${_lang}_FLAGS_RELEASE " /Z7")
    endforeach()
    foreach(_kind EXE MODULE SHARED)
        string(APPEND CMAKE_${_kind}_LINKER_FLAGS_RELEASE " /DEBUG /OPT:REF /OPT:ICF")
    endforeach()
    unset(_lang)
    unset(_kind)
else()
    string(APPEND CMAKE_C_FLAGS_RELEASE " -g")
    string(APPEND CMAKE_CXX_FLAGS_RELEASE " -g")
endif()

# The packages are installed in the configurations Release and Debug, because vcpkg builds no
# other configuration. In a local build as RelWithDebInfo, an imported target without that
# configuration takes the first configuration that it lists unless a mapping is given, which can
# be Debug, whose runtime does not match.
if(NOT DEFINED CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO)
    set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO RelWithDebInfo Release)
endif()
