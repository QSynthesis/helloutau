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
    LINKS HelloUtauEditor HelloKitVoiceBank HelloKitSynth
    QT_LINKS Core Gui Widgets
    INCLUDE_PRIVATE Internal
    MACRO_PREFIX FREQUENCYEDITORPLUGIN
)

# The translations of the plugin, embedded in it under
# :/helloutau/plugins/FrequencyEditor/translations
qt_add_translations(
    TARGETS ${PROJECT_NAME}
    SOURCE_TARGETS ${PROJECT_NAME}
    TS_FILES Resources/translations/FrequencyEditor_zh_CN.ts
    RESOURCE_PREFIX /helloutau/plugins/FrequencyEditor/translations
    LUPDATE_OPTIONS -locations none -no-obsolete
)
