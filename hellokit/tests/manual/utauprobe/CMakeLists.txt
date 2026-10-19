project(utauprobe
    VERSION ${HELLO_VERSION}
    DESCRIPTION "UTAU Probe Project Writer"
    LANGUAGES CXX
)

file(GLOB _src *.h *.cpp)

qm_find_qt(Core)

hellokit_add_executable(${PROJECT_NAME} TEST CONSOLE
    SOURCES ${_src}
    LINKS HelloKitSupport stdcorelib::stdcorelib
    QT_LINKS Core
)
