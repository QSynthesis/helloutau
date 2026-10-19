project(ustrender
    VERSION ${HELLO_VERSION}
    DESCRIPTION "HelloUtau Command-Line Renderer"
    LANGUAGES CXX
)

file(GLOB _src *.h *.cpp)

qm_find_qt(Core)

hellokit_add_executable(${PROJECT_NAME} TEST CONSOLE
    SOURCES ${_src}
    LINKS HelloKitSynth HelloKitDocument stdcorelib::stdcorelib
    QT_LINKS Core
)
