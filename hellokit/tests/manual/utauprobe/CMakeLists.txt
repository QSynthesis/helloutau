project(utauprobe LANGUAGES CXX)

file(GLOB _src *.h *.cpp)

qm_find_qt(Core)

# stdcorelib directly, for the command line parser.
hellokit_add_application(${PROJECT_NAME}
    SOURCES ${_src}
    LINKS HelloKitSupport stdcorelib::stdcorelib
    QT_LINKS Core
)
