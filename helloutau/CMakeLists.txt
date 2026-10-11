project(helloutau
    VERSION ${HELLO_VERSION}
    LANGUAGES CXX
    DESCRIPTION "HelloUtau Editor Libraries"
)

# ----------------------------------
# Project Variables
# ----------------------------------
set(HELLOUTAU_INSTALL ${HELLO_INSTALL})
set(HELLOUTAU_VERSION ${PROJECT_VERSION})
set(HELLOUTAU_INSTALL_NAME ${PROJECT_NAME})

include("conf.cmake")

helloutau_init_buildsystem(HELLOUTAU)

# The definitions of helloutau/Config.h, which helloutau_finish_buildsystem() generates. The
# program sets them as the name, the version and the organization of the application, because
# QCoreApplication finds a version by itself only on Windows and in a macOS bundle.
qm_add_definition(HELLOUTAU_APPLICATION_NAME "HelloUtau" STRING_LITERAL)
qm_add_definition(HELLOUTAU_APPLICATION_VERSION ${PROJECT_VERSION} STRING_LITERAL)
qm_add_definition(HELLOUTAU_ORGANIZATION_NAME "OpenVPI" STRING_LITERAL)

find_package(Qt6 REQUIRED COMPONENTS LinguistTools)

add_subdirectory(lib)

add_subdirectory(plugins)

# The translations of the libraries of both modules, the action manifests included, in one file
# for each language, embedded in HelloUtauEditor under :/helloutau/translations. Each plugin has
# its own (docs/Plugins.md). The update_translations target extracts the texts into the .ts
# files, from the sources that AEC generates as well, so those are built first.
#
# The languages besides English, the same as AppSettings::languages().
set(HELLOUTAU_LANGUAGES zh_CN)
list(TRANSFORM HELLOUTAU_LANGUAGES
    REPLACE "^(.+)$" "lib/Editor/Resources/translations/helloutau_\\1.ts"
    OUTPUT_VARIABLE _ts_files
)
qt_add_translations(
    TARGETS HelloUtauEditor
    SOURCE_TARGETS
        HelloKitDocument HelloKitEdit HelloKitEditBase HelloKitInterchange HelloKitSupport
        HelloKitSynth HelloKitVoiceBank HelloUtauAudio HelloUtauEditor HelloUtauTheme
        HelloUtauWidgets
    TS_FILES ${_ts_files}
    RESOURCE_PREFIX /helloutau/translations
    LUPDATE_OPTIONS -locations none -no-obsolete
)

# The translations of the standard buttons and dialogs of Qt in each language, which
# AppLoader loads from share/Qt/translations beside the bin directory.
if(HELLO_INSTALL)
    foreach(_language IN LISTS HELLOUTAU_LANGUAGES)
        install(FILES "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_TRANSLATIONS}/qtbase_${_language}.qm"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/Qt/translations
        )
    endforeach()
endif()

add_subdirectory(tools)

if(HELLO_BUILD_TESTS)
    add_subdirectory(tests)
endif()

helloutau_install()

helloutau_finish_buildsystem()
