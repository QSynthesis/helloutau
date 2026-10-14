project(HelloKitAudioFile
    VERSION ${HELLO_VERSION}
    LANGUAGES CXX
)

file(GLOB_RECURSE _src *.cpp *.h ../../include/hellokit/AudioFile/*.h)

qm_find_qt(Core)

hellokit_add_library(${PROJECT_NAME} SHARED
    SOURCES ${_src}
    LINKS HelloKitSupport
    LINKS_PRIVATE unofficial::r8brain-free-src::r8bsrc
    QT_LINKS Core
    INCLUDE_PRIVATE ../../include/hellokit/AudioFile/** **
    MACRO_PREFIX HELLOKIT_AUDIOFILE
)

# r8brain-free-src includes windows.h, whose min and max macros would replace std::min and
# std::max.
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)
endif()
