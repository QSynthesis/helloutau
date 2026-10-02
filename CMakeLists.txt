cmake_minimum_required(VERSION 3.19)

set(HELLO_VERSION 0.0.1.0)

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
# Installs what building on the libraries and plugins needs besides the programs: the import and
# static libraries, the headers, the CMake packages, and the debug symbols (PDB on Windows).
option(HELLO_DEVEL "Install the files for development" OFF)
option(HELLO_INSTALL_MSVC_RUNTIME "Install MSVC runtime" OFF)

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
    # Windows has no position-independent code option, and CMake considers clang targeting
    # Windows to be neither MSVC nor MINGW, so this condition must test the platform rather than
    # the compiler.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
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
