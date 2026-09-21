project(HelloKitVoiceBank
    VERSION ${HELLOKIT_VERSION}
    LANGUAGES CXX
)

file(GLOB_RECURSE _src *.cpp *.h ../../include/hellokit/VoiceBank/*.h)

qm_find_qt(Core)

hellokit_add_library(${PROJECT_NAME} SHARED
    SOURCES ${_src}
    LINKS stdutau::stdutau HelloKitSupport
    QT_LINKS Core
    INCLUDE_PRIVATE ../../include/hellokit/VoiceBank/** **
    MACRO_PREFIX HELLOKIT_VOICEBANK
)
