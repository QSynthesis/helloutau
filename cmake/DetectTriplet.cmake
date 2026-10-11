# The triplet of the build, <architecture>-<system>-<ABI>, such as x64-windows-msvc, which names
# the release assets and selects them for an update. See docs/Distribution.md.
#
# The architecture and the system follow the triplets of vcpkg. The ABI rather than the compiler
# is named, because a build of clang for the MSVC ABI loads the plugins of a build of MSVC.
#
# Sets the variable HELLO_TRIPLET.

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|x86_64|x64)$")
    set(_hello_arch x64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|aarch64|arm64)$")
    set(_hello_arch arm64)
else()
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _hello_arch)
endif()

if(WIN32)
    set(_hello_system windows)
elseif(APPLE)
    set(_hello_system osx)
else()
    set(_hello_system linux)
endif()

if(MSVC OR CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    set(_hello_abi msvc)
elseif(MINGW)
    set(_hello_abi mingw)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(_hello_abi gcc)
else()
    string(TOLOWER "${CMAKE_CXX_COMPILER_ID}" _hello_abi)
endif()

set(HELLO_TRIPLET "${_hello_arch}-${_hello_system}-${_hello_abi}")

unset(_hello_arch)
unset(_hello_system)
unset(_hello_abi)
