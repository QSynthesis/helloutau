project(HelloUtauTheme
    VERSION ${HELLOUTAU_VERSION}
    LANGUAGES CXX
)

file(GLOB_RECURSE _src "*.cpp" "*_p.h")
file(GLOB_RECURSE _hdr "${CMAKE_CURRENT_SOURCE_DIR}/../../include/helloutau/Theme/*.h")

qm_find_qt(Core Gui Widgets Svg)

helloutau_add_library(${PROJECT_NAME} SHARED
    QT_AUTOGEN
    SOURCES ${_src} ${_hdr} ThemeIconPlugin.json
    LINKS_PRIVATE stdcorelib::stdcorelib
    QT_LINKS Core Gui Widgets Svg
    INCLUDE_PRIVATE ../../include/** **
    MACRO_PREFIX HELLOUTAU_THEME
)
