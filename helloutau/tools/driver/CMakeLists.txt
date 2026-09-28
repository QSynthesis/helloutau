project(HelloUtau
    DESCRIPTION "HelloUtau Editor"
    LANGUAGES CXX
)

file(GLOB_RECURSE _src "*.cpp")

qm_find_qt(Core Widgets)

set(HELLOUTAU_WINDOWS_APPLICATION ON)
set(HELLOUTAU_CONSOLE_APPLICATION OFF)

helloutau_add_application(${PROJECT_NAME}
    SOURCES ${_src}
    LINKS HelloUtauEditor
    QT_LINKS Core Widgets
)
