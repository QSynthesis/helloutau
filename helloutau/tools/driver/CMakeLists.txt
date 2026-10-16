project(helloutau
    VERSION ${HELLO_VERSION}
    DESCRIPTION "HelloUtau Editor"
    LANGUAGES CXX
)

file(GLOB_RECURSE _src "*.cpp")

qm_find_qt(Core Widgets)

set(HELLOUTAU_WINDOWS_APPLICATION ON)
set(HELLOUTAU_CONSOLE_APPLICATION OFF)

# The file is named in lower case like the other programs, and the Windows resource shows the
# product name.
set(HELLOUTAU_RC_NAME "HelloUtau")

helloutau_add_application(${PROJECT_NAME}
    SOURCES ${_src}
    LINKS HelloUtauEditor
    QT_LINKS Core Widgets
)

# The manifest of qmsetup, without the UTF-8 code page, so that GetACP() reports the ANSI one
qm_add_win_manifest(${PROJECT_NAME} NAME "${HELLOUTAU_RC_NAME}")
