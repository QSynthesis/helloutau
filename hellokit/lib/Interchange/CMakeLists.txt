project(HelloKitInterchange
    VERSION ${HELLOKIT_VERSION}
    LANGUAGES CXX
)

file(GLOB_RECURSE _src "*.cpp")

qm_find_qt(Core)

hellokit_add_library(${PROJECT_NAME} SHARED
    SOURCES ${_src}
    LINKS HelloKitDocument HelloKitSupport
    QT_LINKS Core
    INCLUDE_PRIVATE ../../include/** **
    MACRO_PREFIX HELLOKIT_INTERCHANGE
)
