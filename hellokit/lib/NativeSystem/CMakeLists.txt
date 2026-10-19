project(HelloKitNativeSystem
    VERSION ${HELLO_VERSION}
    LANGUAGES CXX
)

# The facilities of the system without Qt: the native string with its conversion from and to
# UTF-8, the current process and the processes that it starts, and a message box. The application
# uses them before its windows, and hello-updater and hello-plugin-play use them throughout.
set(_src NativeString.cpp NativeMessageBox.cpp)

if(WIN32)
    list(APPEND _src NativeMessageBox_win.cpp NativeProcess_win.cpp)
elseif(APPLE)
    list(APPEND _src NativeMessageBox_mac.cpp NativeProcess_unix.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND _src NativeMessageBox_linux.cpp NativeProcess_unix.cpp)
else()
    message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} is not a system this project runs on.")
endif()

# There is one library for each C runtime of the MSVC ABI, because the objects use the standard
# library, which requires the runtime of the program. HelloKitNativeSystem has the runtime in a
# DLL, as the application and the plugins have it. HelloKitNativeSystem-static-crt has the static
# runtime, as hello-updater and hello-plugin-play link it, and is neither installed nor exported,
# because only these programs and the stand-in of the test of hello-plugin-play link it. Compilers
# of other ABIs ignore the property, and the libraries are alike there.
foreach(_variant dll static-crt)
    if(_variant STREQUAL "dll")
        set(_target ${PROJECT_NAME})
        set(_runtime "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        set(_options)
    else()
        set(_target ${PROJECT_NAME}-static-crt)
        set(_runtime "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set(_options NO_EXPORT NO_INSTALL)
    endif()

    hellokit_add_library(${_target} STATIC ${_options}
        SOURCES ${_src}
        INCLUDE_PRIVATE ../../include/hellokit/NativeSystem/** **
        MACRO_PREFIX HELLOKIT_NATIVESYSTEM
    )

    # The libraries obtain C++17 through Qt, which this target does not link.
    target_compile_features(${_target} PUBLIC cxx_std_17)

    set_target_properties(${_target} PROPERTIES MSVC_RUNTIME_LIBRARY "${_runtime}")

    # The archive is built in the library directory of the tests, which is the library directory
    # itself without a bundle. A static library is no part of the bundle on macOS, which
    # cmake --install copies whole, and codesign rejects an archive in it.
    set_target_properties(${_target} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${HELLOKIT_BUILD_TEST_LIBRARY_DIR}
    )

    if(WIN32)
        target_link_libraries(${_target} PRIVATE user32 advapi32)
    elseif(APPLE)
        target_link_libraries(${_target} PRIVATE "-framework CoreFoundation")
    endif()
endforeach()
