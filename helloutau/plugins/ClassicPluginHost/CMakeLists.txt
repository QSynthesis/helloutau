project(ClassicPluginHostPlugin
    DESCRIPTION "HelloUtau Classic Plugin Host Plugin"
    LANGUAGES CXX
)

file(GLOB_RECURSE _src "*.cpp" "*.h")

qm_find_qt(Core Gui Widgets)

helloutau_add_native_plugin(${PROJECT_NAME}
    ID org.helloutau.classicpluginhost
    DISPLAY_NAME "Classic Plugin Host"
    CATEGORY ClassicPluginHost
    DEPENDENCIES org.helloutau.core
    QT_AUTOGEN
    SOURCES ${_src}
    LINKS HelloUtauEditor HelloKitEdit HelloKitDocument HelloKitVoiceBank HelloKitSynth
          HelloKitSupport
    LINKS_PRIVATE stdutau::stdutau
    QT_LINKS Core Gui Widgets
    INCLUDE_PRIVATE Internal
    MACRO_PREFIX CLASSICPLUGINHOST
)
