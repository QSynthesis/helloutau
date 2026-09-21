project(ustconv LANGUAGES CXX)

file(GLOB _src *.h *.cpp)

qm_find_qt(Core)

# stdcorelib directly, for the command line parser. HelloKitDocument keeps it private, and a
# consumer that wants it says so itself.
hellokit_add_application(${PROJECT_NAME}
    SOURCES ${_src}
    LINKS HelloKitInterchange stdcorelib::stdcorelib
    QT_LINKS Core
)
