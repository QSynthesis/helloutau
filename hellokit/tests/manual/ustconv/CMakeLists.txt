project(ustconv
    DESCRIPTION "HelloUtau Format Converter"
    LANGUAGES CXX
)

file(GLOB _src *.h *.cpp)

qm_find_qt(Core)

hellokit_add_application(${PROJECT_NAME} NO_INSTALL
    SOURCES ${_src}
    LINKS HelloKitInterchange stdcorelib::stdcorelib
    QT_LINKS Core
)
