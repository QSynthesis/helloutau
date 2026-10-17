cmake_minimum_required(VERSION 3.19)

set(HELLO_VERSION 0.1.8.0)

project(helloutau
    VERSION ${HELLO_VERSION}
    LANGUAGES CXX
    DESCRIPTION "Cross-platform UTAU editor"
    HOMEPAGE_URL "https://github.com/QSynthesis/helloutau"
)

# ----------------------------------
# Build Options
# ----------------------------------
option(HELLO_BUILD_TESTS "Build test cases" OFF)
option(HELLO_BUILD_APPLICATION "Build the editor application" ON)
option(HELLO_INSTALL "Install the project" ON)
# Installs the development files besides the programs: the import and static libraries, the
# headers, the CMake packages, and the debug symbols (PDB on Windows).
option(HELLO_DEVEL "Install the files for development" OFF)
# Installs the helper libraries of the tests, which the tests of projects that build on HelloUtau
# link, as LLVM_INSTALL_GTEST of LLVM. Their headers are installed with HELLO_DEVEL. An
# installation for users carries neither these libraries nor Qt Test.
option(HELLO_INSTALL_TESTING "Install the helper libraries of the tests" OFF)
option(HELLO_INSTALL_MSVC_RUNTIME "Install MSVC runtime" OFF)
# Builds the code of this repository with AddressSanitizer, for the tests. The workflow builds it
# as RelWithDebInfo. See the workflow in docs/Distribution.md.
option(HELLO_ENABLE_ADDRESS_SANITIZER "Build with AddressSanitizer" OFF)

# ----------------------------------
# Build Variables
# ----------------------------------
# The channel of the build: release for the tag of a version, ci for the other builds of the
# workflow, and dev for a local build. A build of another channel than release is named by its
# commit in the window titles, the About dialog and the release assets. See docs/Distribution.md.
set(HELLO_BUILD_CHANNEL dev CACHE STRING "The channel of the build: release, ci or dev")
set_property(CACHE HELLO_BUILD_CHANNEL PROPERTY STRINGS release ci dev)

# ----------------------------------
# CMake Settings
# ----------------------------------
if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /manifest:no")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /manifest:no")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /manifest:no")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")

    if(NOT DEFINED CMAKE_DEBUG_POSTFIX)
        set(CMAKE_DEBUG_POSTFIX "d")
    endif()
elseif(MINGW)
    set(CMAKE_STATIC_LIBRARY_PREFIX "")
    set(CMAKE_SHARED_LIBRARY_PREFIX "")
elseif(NOT WIN32)
    # Windows has no position-independent code option, and CMake sets MSVC and MINGW to false for
    # clang targeting Windows, so this condition must test the platform rather than the compiler.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif()

# The packages are installed in the configurations Release and Debug. An imported target without
# a configuration RelWithDebInfo takes the first configuration that it lists unless a mapping
# is given, which can be Debug, whose runtime does not match.
if(NOT DEFINED CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO)
    set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO RelWithDebInfo Release)
endif()

if(HELLO_ENABLE_ADDRESS_SANITIZER)
    # The spelling of the flags follows the front end of the compiler, not the platform: cl takes
    # /fsanitize=address, and clang with the GNU front end takes -fsanitize=address on every
    # system. MSVC supports neither incremental linking nor /RTC with AddressSanitizer.
    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        add_compile_options(/fsanitize=address)
        add_link_options(/INCREMENTAL:NO)
        string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    else()
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()

    # The libraries from vcpkg are built without AddressSanitizer. A container of the standard
    # library of MSVC that is annotated on one side of such a library and not on the other is
    # reported as overflowing, and a static library with other annotations fails to link, so all
    # the annotations are turned off. Overflows into the reserved capacity of std::vector and
    # std::string, and accesses to an empty std::optional, are therefore not detected.
    #
    # _DISABLE_STL_ANNOTATION rather than one macro for each container: the standard library of
    # MSVC 14.51 annotates std::optional as well, which 14.44 does not. With the macros of
    # std::vector and std::string alone, the poison of an std::optional stayed on the stack where
    # code without AddressSanitizer, such as stdutau, had used it, and was later reported as
    # use-after-poison.
    if(WIN32)
        add_compile_definitions(_DISABLE_STL_ANNOTATION)
    endif()
endif()

if(HELLO_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
endif()

# ----------------------------------
# Project Variables
# ----------------------------------
set(HELLO_VERSION ${PROJECT_VERSION})
set(HELLO_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Sets the cache entry HELLO_TRIPLET, the triplet of the build such as x86_64-windows-msvc
include(DetectTriplet)

# ----------------------------------
# Find basic dependencies
# ----------------------------------
find_package(qmsetup REQUIRED)

qm_import(Filesystem)

if(NOT DEFINED QMSETUP_BUILD_DIR)
    set(QMSETUP_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/out")
endif()

qm_init_directories()

# ----------------------------------
# Find third-party dependencies
# ----------------------------------
# Included rather than added as a subdirectory, so that its imported targets are visible to every
# module.
include(${CMAKE_CURRENT_SOURCE_DIR}/third-party/Dependencies.cmake)

# ----------------------------------
# Tests
# ----------------------------------
if(HELLO_BUILD_TESTS)
    enable_testing()
    include(AddAutoTest)
endif()

# ----------------------------------
# Add source modules
# ----------------------------------
add_subdirectory(hellokit)

if(HELLO_BUILD_APPLICATION)
    add_subdirectory(helloutau)
endif()
