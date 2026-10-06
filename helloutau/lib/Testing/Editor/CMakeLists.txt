project(HelloUtauTestingEditor
    VERSION ${HELLO_VERSION}
    LANGUAGES CXX
)

file(GLOB_RECURSE _src "*.cpp" "*_p.h")
file(GLOB_RECURSE _hdr "${CMAKE_CURRENT_SOURCE_DIR}/../../../include/helloutau/Testing/Editor/*.h")

qm_find_qt(Core Gui Widgets Test)

# Installed only if HELLO_INSTALL_TESTING is set, so that an installation for users carries
# neither this library nor Qt Test.
set(_install_option NO_INSTALL)
if(HELLO_INSTALL_TESTING)
    set(_install_option)
endif()

helloutau_add_library(${PROJECT_NAME} SHARED
    SOURCES ${_src} ${_hdr}
    LINKS HelloUtauEditor QActionKit::Widgets
    QT_LINKS Core Gui Widgets Test
    INCLUDE_PRIVATE ../../../include/** **
    MACRO_PREFIX HELLOUTAU_TESTING_EDITOR
    ${_install_option}
)
