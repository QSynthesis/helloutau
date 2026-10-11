project(HelloKitUpdate
    VERSION ${HELLO_VERSION}
    LANGUAGES CXX
)

file(GLOB_RECURSE _src *.cpp *.h ../../include/hellokit/Update/*.h)

# The installation records of the Windows installer
if(NOT WIN32)
    list(FILTER _src EXCLUDE REGEX "/Windows/")
endif()

qm_find_qt(Core Network)

hellokit_add_library(${PROJECT_NAME} SHARED
    SOURCES ${_src}
    LINKS_PRIVATE stdcorelib::stdcorelib
    QT_LINKS Core
    QT_LINKS_PRIVATE Network
    QT_AUTOGEN
    INCLUDE_PRIVATE ../../include/hellokit/Update/** **
    MACRO_PREFIX HELLOKIT_UPDATE
)
