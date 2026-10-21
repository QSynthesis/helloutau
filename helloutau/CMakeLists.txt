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

# HelloUtau.app on macOS, see the root CMakeLists.txt
set(HELLOUTAU_MACOSX_BUNDLE_NAME ${HELLO_MACOSX_BUNDLE_NAME})

helloutau_init_buildsystem(HELLOUTAU)

# The definitions of helloutau/Config.h, which helloutau_finish_buildsystem() generates. The
# program sets them as the name and the organization of the application. The version and the
# triplet of the build are in hellokit/Config.h, and the build metadata in hellokit/BuildInfo.h,
# because the programs of hellokit read them as well.
qm_add_definition(HELLOUTAU_APPLICATION_NAME "HelloUtau" STRING_LITERAL)
qm_add_definition(HELLOUTAU_ORGANIZATION_NAME "OpenVPI" STRING_LITERAL)

# The identifier of the application: the AppId of the installer of Windows, the bundle identifier
# of macOS, and on Linux the name of the desktop entry and of the icon, which the program sets as
# the desktop file name. It never changes after the first release, see docs/Distribution.md.
set(HELLOUTAU_APPLICATION_ID org.openvpi.helloutau)
qm_add_definition(HELLOUTAU_APPLICATION_ID "${HELLOUTAU_APPLICATION_ID}" STRING_LITERAL)

# The repositories of the releases and the identifiers of the assets and of the installer, which
# UpdateChecker reads from update in the data directory of qmsetup, share/helloutau or
# Contents/Resources of a macOS bundle, in the build as in the installation. See the automatic
# update in docs/Distribution.md.
configure_file(share/helloutau/update/UpdateManifest.json
    ${HELLOUTAU_BUILD_DATA_DIR}/update/UpdateManifest.json COPYONLY
)
if(HELLO_INSTALL)
    install(FILES share/helloutau/update/UpdateManifest.json
        DESTINATION ${HELLOUTAU_INSTALL_DATA_DIR}/update)

    # The license in the data directory, so that every package and installation carries it
    install(FILES ${HELLO_SOURCE_DIR}/LICENSE DESTINATION ${HELLOUTAU_INSTALL_DATA_DIR})
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

# Deploys Qt and the other dependencies after every other install rule, with the plugin choices of
# windeployqt for a program without QML. Not for Debug, because qm_deploy_directory deploys
# release libraries only. See the installer in docs/Distribution.md.
if(HELLO_INSTALL AND HELLO_DEPLOY AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    qm_import(Deploy)

    # The libraries go beside the programs on Windows and into the library directory elsewhere,
    # Contents/Frameworks of the bundle on macOS. The plugins of Qt go into Qt/plugins of the
    # library directory, where AppLoader::qtPluginPaths() finds them.
    if(WIN32)
        set(_library_dir ${HELLOUTAU_INSTALL_RUNTIME_DIR})
        set(_qt_plugin_dir lib/Qt/plugins)
        set(_searching_paths "${QMSETUP_BUILD_DIR}/${HELLOUTAU_INSTALL_RUNTIME_DIR}")
    else()
        set(_library_dir ${HELLOUTAU_INSTALL_LIBRARY_DIR})
        set(_qt_plugin_dir ${HELLOUTAU_INSTALL_LIBRARY_DIR}/Qt/plugins)
        set(_searching_paths "${HELLOUTAU_BUILD_LIBRARY_DIR}")
    endif()

    # The libraries of vcpkg, which are shared on every system and which only Windows copies
    # into the build, see third-party/Dependencies.cmake
    if(DEFINED _VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET AND NOT WIN32)
        list(APPEND _searching_paths "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib")
    endif()

    # libc++ and libunwind of llvm-mingw, beside the compiler and in the directory of the target
    if(MINGW)
        get_filename_component(_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        list(APPEND _searching_paths "${_compiler_dir}")

        if(CMAKE_CXX_COMPILER_TARGET)
            set(_target_triple "${CMAKE_CXX_COMPILER_TARGET}")
        else()
            execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -dumpmachine
                OUTPUT_VARIABLE _target_triple OUTPUT_STRIP_TRAILING_WHITESPACE
            )
        endif()

        if(_target_triple AND IS_DIRECTORY "${_compiler_dir}/../${_target_triple}/bin")
            list(APPEND _searching_paths "${_compiler_dir}/../${_target_triple}/bin")
        endif()
    endif()

    # Linux: Qt names xcb only, and the program then runs through XWayland in a Wayland session,
    # blurred at a fractional scale. With wayland, the plugins of the types of the Wayland
    # client serve the platform plugin. The device integrations serve the platform eglfs, which
    # is not deployed.
    set(_qt_plugin_options)
    set(_qt_plugins_exclude tls/qopensslbackend generic/qinsighttracker)
    if(NOT WIN32 AND NOT APPLE)
        list(APPEND _qt_plugin_options QT_PLUGINS_PLATFORMS xcb wayland)
        list(APPEND _qt_plugins_exclude
            egldeviceintegrations/qeglfs-emu-integration
            egldeviceintegrations/qeglfs-kms-egldevice-integration
            egldeviceintegrations/qeglfs-kms-integration
            egldeviceintegrations/qeglfs-x11-integration
        )
    endif()

    qm_deploy_directory(.
        LIBRARY_DIR ${_library_dir}
        PLUGIN_DIR ${_qt_plugin_dir}
        EXTRA_SEARCHING_PATHS ${_searching_paths}
        QT_PLUGINS
        QT_PLUGINS_EXCLUDE_MODULES Qml Quick
        QT_PLUGINS_EXCLUDE ${_qt_plugins_exclude}
        ${_qt_plugin_options}
        STANDARD
        VERBOSE
        COMMENT "Deploying Qt and the libraries that the installed binaries require"
    )
endif()

helloutau_finish_buildsystem()
