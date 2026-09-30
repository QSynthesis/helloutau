project(InterchangePlugin
    DESCRIPTION "HelloUtau Interchange Plugin"
    LANGUAGES CXX
)

file(GLOB_RECURSE _src "*.cpp" "*.h")

qm_find_qt(Core)

helloutau_add_native_plugin(${PROJECT_NAME}
    ID org.helloutau.interchange
    DISPLAY_NAME Interchange
    CATEGORY Interchange
    DEPENDENCIES org.helloutau.core
    SOURCES ${_src}
    LINKS HelloKitInterchange
    QT_LINKS Core
    INCLUDE_PRIVATE Internal
)
