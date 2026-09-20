project(HelloUtauWidgets
    VERSION ${HELLOUTAU_VERSION}
    LANGUAGES CXX
)

file(GLOB_RECURSE _src "*.cpp")
file(GLOB_RECURSE _hdr "${CMAKE_CURRENT_SOURCE_DIR}/../../include/helloutau/Widgets/*.h")

qm_find_qt(Core Gui Widgets)

helloutau_add_library(${PROJECT_NAME} SHARED
    QT_AUTOGEN
    SOURCES ${_src} ${_hdr}
    LINKS HelloKitUst
    QT_LINKS Core Gui Widgets
    INCLUDE_PRIVATE ../../include/** **
    MACRO_PREFIX HELLOUTAU_WIDGETS
)
