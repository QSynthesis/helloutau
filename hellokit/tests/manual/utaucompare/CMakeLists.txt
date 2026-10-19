project(utaucompare
    VERSION ${HELLO_VERSION}
    DESCRIPTION "HelloUtau and UTAU Synth Tool Call Comparison"
    LANGUAGES CXX
)

file(GLOB _src *.h *.cpp)

qm_find_qt(Core)

hellokit_add_executable(${PROJECT_NAME} TEST CONSOLE
    SOURCES ${_src}
    LINKS HelloKitSynth HelloKitDocument HelloKitSupport stdcorelib::stdcorelib
    QT_LINKS Core
)
