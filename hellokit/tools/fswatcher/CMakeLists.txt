project(hello-fswatcher LANGUAGES CXX)

# What tells hello::kit::FileSystemWatcher which directories on disk may have changed. A process
# of its own, so that whatever the system's notifications do to it, a crash, a hang or a limit
# run into, stays out of the editor. See FileSystemWatcher.h for the rest of why.
#
# No Qt: it is started once per watcher and has to be small and quick to start. Exported, so
# that a consumer can find where it was installed.
#
# One backend per system, chosen here: ReadDirectoryChangesW, FSEvents, inotify. Any other
# system gets Backend_unix.cpp, which follows nothing and answers every root as unwatchable.
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
    list(APPEND _src Backend_unix.cpp)
endif()

hellokit_add_executable(${PROJECT_NAME}
    SOURCES ${_src}
    CONSOLE
)

# The libraries get C++17 by way of Qt, and nothing links Qt here.
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework CoreServices")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
endif()
