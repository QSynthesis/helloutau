project(CorePlugin
    VERSION ${HELLO_VERSION}
    DESCRIPTION "HelloUtau Core Plugin"
    LANGUAGES CXX
)

file(GLOB_RECURSE _src "*.cpp" "*.h")

qm_find_qt(Core Widgets)

helloutau_add_native_plugin(${PROJECT_NAME}
    ID org.helloutau.core
    DISPLAY_NAME Core
    DESCRIPTION "Provides the built-in HelloUtau editor features."
    CATEGORY Core
    QT_AUTOGEN
    SOURCES ${_src}
    LINKS HelloUtauEditor
    LINKS_PRIVATE QActionKit::Widgets
    QT_LINKS Core Widgets
    INCLUDE_PRIVATE Internal
    MACRO_PREFIX COREPLUGIN
)

# The translations of the plugin, embedded in it under :/helloutau/plugins/Core/translations
qt_add_translations(
    TARGETS ${PROJECT_NAME}
    SOURCE_TARGETS ${PROJECT_NAME}
    TS_FILES translations/Core_zh_CN.ts
    RESOURCE_PREFIX /helloutau/plugins/Core/translations
    LUPDATE_OPTIONS -locations none -no-obsolete
)
