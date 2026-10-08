project(HelloKitInterchange
    VERSION ${HELLO_VERSION}
    LANGUAGES CXX
)

file(GLOB_RECURSE _src *.cpp *.h ../../include/hellokit/Interchange/*.h)

qm_find_qt(Core)

hellokit_add_library(${PROJECT_NAME} SHARED
    SOURCES ${_src}
    LINKS HelloKitDocument stdcorelib::stdcorelib
    LINKS_PRIVATE wolf-midi::wolf-midi
    QT_LINKS Core
    QT_AUTOGEN
    INCLUDE_PRIVATE ../../include/hellokit/Interchange/** **
    MACRO_PREFIX HELLOKIT_INTERCHANGE
)
