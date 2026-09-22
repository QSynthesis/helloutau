project(HelloKitSupport
    VERSION ${HELLOKIT_VERSION}
    LANGUAGES CXX
)

file(GLOB_RECURSE _src *.cpp *.h ../../include/hellokit/Support/*.h)

qm_find_qt(Core)

hellokit_add_library(${PROJECT_NAME} SHARED
    SOURCES ${_src}
    LINKS_PRIVATE winacp::winacp
    QT_LINKS Core
    QT_AUTOGEN
    INCLUDE_PRIVATE ../../include/hellokit/Support/** **
    MACRO_PREFIX HELLOKIT_SUPPORT
)
