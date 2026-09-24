project(utauprobe
    DESCRIPTION "UTAU Probe Project Writer"
    LANGUAGES CXX
)

file(GLOB _src *.h *.cpp)

qm_find_qt(Core)

hellokit_add_application(${PROJECT_NAME} NO_INSTALL
    SOURCES ${_src}
    LINKS HelloKitSupport stdcorelib::stdcorelib
    QT_LINKS Core
)
