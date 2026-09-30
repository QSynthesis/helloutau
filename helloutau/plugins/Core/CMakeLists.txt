project(coreplugin
    DESCRIPTION "HelloUtau Core Plugin"
    LANGUAGES CXX
)

file(GLOB_RECURSE _src "*.cpp")

qm_find_qt(Core Widgets)

helloutau_add_native_plugin(${PROJECT_NAME}
    ID org.helloutau.core
    DISPLAY_NAME Core
    CATEGORY Core
    SOURCES ${_src}
    LINKS HelloUtauEditor
    QT_LINKS Core Widgets
)
