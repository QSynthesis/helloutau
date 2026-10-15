project(HelloKitWinMain
    VERSION ${HELLO_VERSION}
    LANGUAGES CXX
)

# The entry point of a program of the Windows subsystem whose source defines main, as
# Qt6::EntryPoint provides it, with the arguments in UTF-8. A program links the library on Windows
# and needs neither a header nor a linker option of its own. See EntryPoint.cpp.
file(GLOB_RECURSE _src *.cpp)

# The library serves programs that link the C runtime statically, as hello-updater does, and
# programs that link it as a DLL. Its objects therefore name no runtime library: the target selects
# none, and cl, which otherwise names its default, is told not to. The source calls only the
# Windows API, so that the objects require no particular runtime.
set(CMAKE_MSVC_RUNTIME_LIBRARY "")

hellokit_add_library(${PROJECT_NAME} STATIC
    SOURCES ${_src}
    LINKS_PRIVATE shell32
    MACRO_PREFIX HELLOKIT_WINMAIN
)

if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(${PROJECT_NAME} PRIVATE /Zl)
endif()

# MinGW-w64 starts main itself, also in a program of the Windows subsystem. -municode makes it
# start wmain instead, which EntryPoint.cpp defines there.
if(MINGW)
    target_link_options(${PROJECT_NAME} INTERFACE -municode)
endif()
