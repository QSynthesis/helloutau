project(SynthToolSupportPlugin
    VERSION ${HELLO_VERSION}
    DESCRIPTION "HelloUtau Synth Tool Support Plugin"
    LANGUAGES CXX
)

file(GLOB_RECURSE _src "*.cpp" "*.h")

qm_find_qt(Core Gui Widgets)

helloutau_add_native_plugin(${PROJECT_NAME}
    ID org.helloutau.synthtoolsupport
    DISPLAY_NAME "Synth Tool Support"
    DESCRIPTION "Supports the frequency maps and the failures of third-party synth tools."
    CATEGORY SynthToolSupport
    DEPENDENCIES org.helloutau.core
    SOURCES ${_src}
    LINKS HelloUtauEditor HelloKitVoiceBank HelloKitSynth
    QT_LINKS Core Gui Widgets
    INCLUDE_PRIVATE Internal
    MACRO_PREFIX SYNTHTOOLSUPPORTPLUGIN
)

# The translations of the plugin, embedded in it under
# :/helloutau/plugins/SynthToolSupport/translations
qt_add_translations(
    TARGETS ${PROJECT_NAME}
    SOURCE_TARGETS ${PROJECT_NAME}
    TS_FILES Resources/translations/SynthToolSupport_zh_CN.ts
    RESOURCE_PREFIX /helloutau/plugins/SynthToolSupport/translations
    LUPDATE_OPTIONS -locations none -no-obsolete
)
