project(hello-fswatcher LANGUAGES CXX)

# The program that reports possibly changed directories to hello::kit::FileSystemWatcher. It runs
# as a separate process, so that failures of the system notification facilities, such as
# crashes, hangs and resource limits, do not affect the editor. See FileSystemWatcher.h for the
# full rationale.
#
# Without Qt, because one instance is started per watcher and it must be small and start
# quickly. Exported, so that a consumer can locate the installed program.
#
# One backend per system, selected here: ReadDirectoryChangesW, FSEvents or inotify. These are
# the systems this project supports, and configuration fails on any other system rather than
# producing a partial build.
set(_src
    Backend.h
    Protocol.h
    Protocol.cpp
    main.cpp
)

if(WIN32)
    list(APPEND _src Backend_win.cpp)
elseif(APPLE)
    list(APPEND _src Backend_mac.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND _src Backend_linux.cpp)
else()
    message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} is not a system this project runs on. "
        "hello-fswatcher follows the disk on Windows, macOS and Linux.")
endif()

hellokit_add_executable(${PROJECT_NAME}
    SOURCES ${_src}
    CONSOLE
)

# The libraries obtain C++17 through Qt, which this target does not link.
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework CoreServices")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
endif()
