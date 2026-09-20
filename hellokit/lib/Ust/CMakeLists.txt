project(HelloKitUst
    VERSION ${HELLOKIT_VERSION}
    LANGUAGES CXX
)

file(GLOB_RECURSE _src "*.cpp")

find_package(stdcorelib CONFIG REQUIRED)

qm_find_qt(Core)

hellokit_add_library(${PROJECT_NAME} SHARED
    SOURCES ${_src}
    LINKS stdcorelib::stdcorelib stdutau::stdutau
    QT_LINKS Core
    INCLUDE_PRIVATE ../../include/** **
    MACRO_PREFIX HELLOKIT_UST
)
