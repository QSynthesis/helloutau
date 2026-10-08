project(FrequencyEditorPlugin
    VERSION ${HELLO_VERSION}
    DESCRIPTION "HelloUtau Frequency Editor Plugin"
    LANGUAGES CXX
)

file(GLOB_RECURSE _src "*.cpp" "*.h")

qm_find_qt(Core Gui Widgets)

helloutau_add_native_plugin(${PROJECT_NAME}
    ID org.helloutau.frequencyeditor
    DISPLAY_NAME "Frequency Editor"
    DESCRIPTION "Edits frequency tables for voice banks."
    CATEGORY FrequencyEditor
    DEPENDENCIES org.helloutau.core
    SOURCES ${_src}
    LINKS HelloUtauEditor HelloKitVoiceBank
    QT_LINKS Core Gui Widgets
    INCLUDE_PRIVATE Internal
)
