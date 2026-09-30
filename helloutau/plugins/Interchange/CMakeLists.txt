project(InterchangePlugin
    DESCRIPTION "HelloUtau Interchange Plugin"
    LANGUAGES CXX
)

file(GLOB_RECURSE _src "*.cpp" "*.h")

# Import and Export → Other Formats of the File menu, compiled by AEC into
# InterchangeActions.qak.h
qak_add_action_extension(_actions_src Internal/InterchangeActions.xml
    FUNCTION interchangeActions
    NAMESPACE hello::daw
)

qm_find_qt(Core Gui Widgets)

helloutau_add_native_plugin(${PROJECT_NAME}
    ID org.helloutau.interchange
    DISPLAY_NAME Interchange
    CATEGORY Interchange
    DEPENDENCIES org.helloutau.core
    QT_AUTOGEN
    SOURCES ${_src} ${_actions_src}
    LINKS HelloUtauEditor HelloKitInterchange HelloKitEdit HelloKitDocument HelloKitSupport
    LINKS_PRIVATE QActionKit::Widgets
    QT_LINKS Core Gui Widgets
    INCLUDE_PRIVATE Internal
    MACRO_PREFIX INTERCHANGEPLUGIN
)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
