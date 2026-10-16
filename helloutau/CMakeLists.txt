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
# program sets them as the name and the organization of the application. The version and the
# triplet of the build are in hellokit/Config.h, and the build metadata in hellokit/BuildInfo.h,
# because the programs of hellokit read them as well.
qm_add_definition(HELLOUTAU_APPLICATION_NAME "HelloUtau" STRING_LITERAL)
qm_add_definition(HELLOUTAU_ORGANIZATION_NAME "OpenVPI" STRING_LITERAL)

# The repositories of the releases and the identifiers of the assets and of the installer, which
# UpdateChecker reads from share/helloutau/update beside the bin directory, in the build as in
# the installation. See the automatic update in docs/Distribution.md.
configure_file(share/helloutau/update/UpdateManifest.json
    ${QMSETUP_BUILD_DIR}/share/helloutau/update/UpdateManifest.json COPYONLY
)
if(HELLO_INSTALL)
    install(FILES share/helloutau/update/UpdateManifest.json
        DESTINATION ${CMAKE_INSTALL_DATADIR}/helloutau/update)
endif()

find_package(Qt6 REQUIRED COMPONENTS LinguistTools)

add_subdirectory(lib)

add_subdirectory(plugins)

# The translations of the libraries of both modules, the action manifests included, in one file
# for each language, in the translations of the data directory of qmsetup, share/helloutau or
# Contents/Resources of a macOS bundle, in the build as in the installation, as Qt Creator keeps
# share/qtcreator/translations. AppLoader::translationPaths() finds them. Each plugin has its own
# (docs/Plugins.md). The update_translations target extracts the texts into the .ts files, from
# the sources that AEC generates as well, so those are built first.
#
# The languages besides English, the same as AppearanceSettings::languages().
set(HELLOUTAU_LANGUAGES zh_CN)
list(TRANSFORM HELLOUTAU_LANGUAGES
    REPLACE "^(.+)$" "share/helloutau/translations/helloutau_\\1.ts"
    OUTPUT_VARIABLE _ts_files
)
set_source_files_properties(${_ts_files} PROPERTIES
    OUTPUT_LOCATION ${HELLOUTAU_BUILD_DATA_DIR}/translations
)
# QM_FILES_OUTPUT_VARIABLE rather than RESOURCE_PREFIX, so that the files are not embedded
qt_add_translations(
    TARGETS HelloUtauEditor
    SOURCE_TARGETS
        HelloKitAudioFile HelloKitDocument HelloKitEdit HelloKitEditBase HelloKitInterchange
        HelloKitSupport
        HelloKitSynth HelloKitUpdate HelloKitVoiceBank HelloUtauAudio HelloUtauEditor HelloUtauTheme
        HelloUtauWidgets
    TS_FILES ${_ts_files}
    QM_FILES_OUTPUT_VARIABLE _qm_files
    LRELEASE_TARGET helloutau_translations
    LUPDATE_OPTIONS -locations none -no-obsolete
)
# Built with the library that loads them rather than only with the target all, so that building
# the program alone yields them
add_dependencies(HelloUtauEditor helloutau_translations)
if(HELLO_INSTALL)
    install(FILES ${_qm_files} DESTINATION ${HELLOUTAU_INSTALL_DATA_DIR}/translations)
endif()

# The translations of the standard buttons and dialogs of Qt in each language, in Qt/translations
# of the shared directory of qmsetup, share or Contents/Resources of a macOS bundle, which
# AppLoader::qtTranslationPaths() finds.
if(HELLO_INSTALL)
    foreach(_language IN LISTS HELLOUTAU_LANGUAGES)
        install(FILES "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_TRANSLATIONS}/qtbase_${_language}.qm"
            DESTINATION ${HELLOUTAU_INSTALL_SHARE_DIR}/Qt/translations
        )
    endforeach()
endif()

add_subdirectory(tools)

if(HELLO_BUILD_TESTS)
    add_subdirectory(tests)
endif()

helloutau_install()

helloutau_finish_buildsystem()
