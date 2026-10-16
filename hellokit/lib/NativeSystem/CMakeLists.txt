project(HelloKitNativeSystem
    VERSION ${HELLO_VERSION}
    LANGUAGES CXX
)

# The facilities of the system for the programs that link no Qt, such as hello-updater and
# hello-plugin-play: the native string with its conversion from and to UTF-8, and a message box.
set(_src NativeString.cpp)

if(WIN32)
    list(APPEND _src NativeMessageBox_win.cpp)
elseif(APPLE)
    list(APPEND _src NativeMessageBox_mac.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND _src NativeMessageBox_linux.cpp)
else()
    message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} is not a system this project runs on.")
endif()

hellokit_add_library(${PROJECT_NAME} STATIC
    SOURCES ${_src}
    INCLUDE_PRIVATE ../../include/hellokit/NativeSystem/** **
    MACRO_PREFIX HELLOKIT_NATIVESYSTEM
)

# The libraries obtain C++17 through Qt, which this target does not link.
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

# The static runtime of the MSVC ABI, as the programs that link the library use it. The objects
# use the standard library, which requires the runtime of the program.
set_target_properties(${PROJECT_NAME} PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE user32)
elseif(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework CoreFoundation")
endif()
