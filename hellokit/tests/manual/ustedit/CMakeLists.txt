project(ustedit LANGUAGES CXX)

file(GLOB _src *.h *.cpp)

qm_find_qt(Core)

# Links stdcorelib directly for the command-line parser. HelloKitDocument links it privately,
# so a consumer that requires it must link it explicitly.
hellokit_add_application(${PROJECT_NAME} NO_INSTALL
    SOURCES ${_src}
    LINKS HelloKitEdit stdcorelib::stdcorelib
    QT_LINKS Core
)
