project(HelloUtauAudio
    VERSION ${HELLOUTAU_VERSION}
    LANGUAGES CXX
)

file(GLOB_RECURSE _src "*.cpp" "*_p.h")
file(GLOB_RECURSE _hdr "${CMAKE_CURRENT_SOURCE_DIR}/../../include/helloutau/Audio/*.h")

qm_find_qt(Core Multimedia)

helloutau_add_library(${PROJECT_NAME} SHARED
    QT_AUTOGEN
    SOURCES ${_src} ${_hdr}
    LINKS_PRIVATE unofficial::r8brain-free-src::r8bsrc
    QT_LINKS Core Multimedia
    INCLUDE_PRIVATE ../../include/** **
    MACRO_PREFIX HELLOUTAU_AUDIO
)

# r8brain-free-src includes windows.h, whose min and max macros would replace std::min and
# std::max.
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)
endif()
