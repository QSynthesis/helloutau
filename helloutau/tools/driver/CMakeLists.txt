project(helloutau
    VERSION ${HELLO_VERSION}
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

# The resource with the icon and the product name, which conf.cmake leaves to an application
qm_add_win_rc(${PROJECT_NAME}
    NAME "HelloUtau"
    DESCRIPTION "${PROJECT_DESCRIPTION}"
    COPYRIGHT "${HELLOUTAU_RC_COPYRIGHT}"
    ICON "${CMAKE_CURRENT_SOURCE_DIR}/helloutau.ico"
)

# The manifest of qmsetup, without the UTF-8 code page, so that GetACP() reports the ANSI one
qm_add_win_manifest(${PROJECT_NAME} NAME "HelloUtau")
