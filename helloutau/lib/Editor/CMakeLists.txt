project(HelloUtauEditor
    VERSION ${HELLOUTAU_VERSION}
    LANGUAGES CXX
)

file(GLOB_RECURSE _src "*.cpp" "*_p.h")
file(GLOB_RECURSE _hdr "${CMAKE_CURRENT_SOURCE_DIR}/../../include/helloutau/Editor/*.h")

qm_find_qt(Core Gui Widgets)

# The menus of the editor window, compiled by AEC into EditorActions.qak.h and its source
qak_add_action_extension(_actions_src EditorActions.xml
    FUNCTION editorActions
    NAMESPACE hello::daw
)

# The built-in theme, under :/helloutau/themes
set(_themes_qrc themes/Themes.qrc)

helloutau_add_library(${PROJECT_NAME} SHARED
    QT_AUTOGEN
    SOURCES ${_src} ${_hdr} ${_actions_src} ${_themes_qrc}
    LINKS HelloUtauWidgets HelloUtauTheme HelloUtauAudio HelloKitEdit HelloKitDocument
          HelloKitVoiceBank HelloKitSynth
    LINKS_PRIVATE QActionKit::Widgets
    QT_LINKS Core Gui Widgets
    INCLUDE_PRIVATE ../../include/** **
    MACRO_PREFIX HELLOUTAU_EDITOR
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
