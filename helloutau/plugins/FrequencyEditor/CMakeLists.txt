project(FrequencyEditorPlugin
    DESCRIPTION "HelloUtau Frequency Editor Plugin"
    LANGUAGES CXX
)

file(GLOB_RECURSE _src "*.cpp" "*.h")

qm_find_qt(Core)

helloutau_add_native_plugin(${PROJECT_NAME}
    ID org.helloutau.frequencyeditor
    DISPLAY_NAME "Frequency Editor"
    CATEGORY FrequencyEditor
    DEPENDENCIES org.helloutau.core
    SOURCES ${_src}
    LINKS HelloKitVoiceBank
    QT_LINKS Core
    INCLUDE_PRIVATE Internal
)
