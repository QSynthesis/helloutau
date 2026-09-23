project(utaucompare LANGUAGES CXX)

file(GLOB _src *.h *.cpp)

qm_find_qt(Core)

# Links stdcorelib directly for the command-line parser.
hellokit_add_application(${PROJECT_NAME} NO_INSTALL
    SOURCES ${_src}
    LINKS HelloKitSynth HelloKitDocument HelloKitSupport stdcorelib::stdcorelib
    QT_LINKS Core
)
