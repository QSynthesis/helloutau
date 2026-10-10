project(ClassicPluginHostPlugin
    VERSION ${HELLO_VERSION}
    DESCRIPTION "HelloUtau Classic Plugin Host Plugin"
    LANGUAGES CXX
)

file(GLOB_RECURSE _src "*.cpp" "*.h")

# The submenu Plugins of the Tools menu, compiled by AEC into ClassicPluginActions.qak.h
qak_add_action_extension(_actions_src Resources/ClassicPluginActions.xml
    FUNCTION classicPluginActions
    NAMESPACE hello::daw
)

qm_find_qt(Core Gui Widgets)

helloutau_add_native_plugin(${PROJECT_NAME}
    ID org.helloutau.classicpluginhost
    DISPLAY_NAME "Classic Plugin Host"
    DESCRIPTION "Runs classic UTAU plugins."
    CATEGORY ClassicPluginHost
    DEPENDENCIES org.helloutau.core
    QT_AUTOGEN
    SOURCES ${_src} ${_actions_src}
    LINKS HelloUtauEditor HelloKitEdit HelloKitDocument HelloKitVoiceBank HelloKitSynth
          HelloKitSupport
    LINKS_PRIVATE stdutau::stdutau QActionKit::Widgets
    QT_LINKS Core Gui Widgets
    INCLUDE_PRIVATE Internal
    MACRO_PREFIX CLASSICPLUGINHOST
)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# The translations of the plugin, embedded in it under
# :/helloutau/plugins/ClassicPluginHost/translations
qt_add_translations(
    TARGETS ${PROJECT_NAME}
    SOURCE_TARGETS ${PROJECT_NAME}
    TS_FILES Resources/translations/ClassicPluginHost_zh_CN.ts
    RESOURCE_PREFIX /helloutau/plugins/ClassicPluginHost/translations
    LUPDATE_OPTIONS -locations none -no-obsolete
)
