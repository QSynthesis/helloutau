project(ustedit
    DESCRIPTION "HelloUtau Command-Line Project Editor"
    LANGUAGES CXX
)

file(GLOB _src *.h *.cpp)

qm_find_qt(Core)

hellokit_add_application(${PROJECT_NAME} NO_INSTALL
    SOURCES ${_src}
    LINKS HelloKitEdit stdcorelib::stdcorelib
    QT_LINKS Core
)
