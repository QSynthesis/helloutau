project(ustrender
    DESCRIPTION "HelloUtau Command-Line Renderer"
    LANGUAGES CXX
)

file(GLOB _src *.h *.cpp)

qm_find_qt(Core)

hellokit_add_application(${PROJECT_NAME} NO_INSTALL
    SOURCES ${_src}
    LINKS HelloKitSynth HelloKitDocument stdcorelib::stdcorelib
    QT_LINKS Core
)
