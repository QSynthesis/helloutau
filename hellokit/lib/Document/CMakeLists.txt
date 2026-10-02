project(HelloKitDocument
    VERSION ${HELLO_VERSION}
    LANGUAGES CXX
)

file(GLOB_RECURSE _src *.cpp *.h ../../include/hellokit/Document/*.h)

qm_find_qt(Core)

hellokit_add_library(${PROJECT_NAME} SHARED
    SOURCES ${_src}
    LINKS stdutau::stdutau HelloKitSupport
    LINKS_PRIVATE stdcorelib::stdcorelib
    QT_LINKS Core
    INCLUDE_PRIVATE ../../include/hellokit/Document/** **
    MACRO_PREFIX HELLOKIT_DOCUMENT
)
