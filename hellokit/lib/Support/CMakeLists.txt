project(HelloKitSupport
    VERSION ${HELLO_VERSION}
    LANGUAGES CXX
)

file(GLOB_RECURSE _src *.cpp *.h ../../include/hellokit/Support/*.h)

qm_find_qt(Core Network)

# Network for the local server of SingleProcessPeer
hellokit_add_library(${PROJECT_NAME} SHARED
    SOURCES ${_src}
    LINKS stdcorelib::stdcorelib
    LINKS_PRIVATE winacp::winacp libzip::zip
    QT_LINKS Core
    QT_LINKS_PRIVATE Network
    QT_AUTOGEN
    INCLUDE_PRIVATE ../../include/hellokit/Support/** **
    MACRO_PREFIX HELLOKIT_SUPPORT
)

# FileManager selects a file in File Explorer through the shell and COM.
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE shell32 ole32)
endif()
