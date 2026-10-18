cmake_minimum_required(VERSION 3.19)

# The oldest macOS that the application supports, 13.3, which stdutau requires, set before
# project() so that the compilers receive it. See the release of macOS in docs/Distribution.md.
set(CMAKE_OSX_DEPLOYMENT_TARGET 13.3 CACHE STRING "The oldest macOS that the build supports")

project(helloutau
    VERSION 0.1.8.0
    LANGUAGES CXX
    DESCRIPTION "Modern cross-platform UTAU editor with faithful compatibility"
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
# Deploys Qt and the other libraries that the installed binaries require into the installation,
# not for a Debug build, see helloutau/CMakeLists.txt. An installation for development against
# libraries elsewhere, or into a system prefix such as /usr/local, turns it off.
option(HELLO_DEPLOY "Deploy the dependencies into the installation" ON)
# Builds the code of this repository with AddressSanitizer, for the tests. The workflow builds it
# as Release, which carries the debug information. See the workflow in docs/Distribution.md.
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
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

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

# The debug information of Release and the mapping of the imported targets for RelWithDebInfo
include(ConfigureBuildTypes)

if(HELLO_ENABLE_ADDRESS_SANITIZER)
    include(EnableAddressSanitizer)
endif()

if(HELLO_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
endif()

# ----------------------------------
# Project Variables
# ----------------------------------
# The version of the repository, which every subdirectory passes to its own project(). That
# project() replaces PROJECT_VERSION, and an ordinary variable reaches every subdirectory.
set(HELLO_VERSION ${PROJECT_VERSION})
set(HELLO_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Sets the cache entry HELLO_TRIPLET, the triplet of the build such as x86_64-windows-msvc
include(DetectTriplet)

# The application is a bundle on macOS, HelloUtau.app. Both modules lay their build and their
# installation out in it, as qmsetup does for <proj>_MACOSX_BUNDLE_NAME: the programs in
# Contents/MacOS, the libraries in Contents/Frameworks, the plugins in Contents/Plugins and the
# data in Contents/Resources.
set(HELLO_MACOSX_BUNDLE_NAME)
if(APPLE AND HELLO_BUILD_APPLICATION)
    set(HELLO_MACOSX_BUNDLE_NAME HelloUtau)
endif()

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
