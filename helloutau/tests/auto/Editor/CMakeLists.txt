helloutau_add_auto_test(test_ActionLayoutsFile.cpp HelloUtauEditor QActionKit::Core)
helloutau_add_auto_test(test_AppSettings.cpp HelloUtauEditor)
helloutau_add_auto_test(test_FrequencyMapView.cpp HelloUtauEditor)
helloutau_add_auto_test(test_FrequencyMapWaveform.cpp HelloUtauEditor)
helloutau_add_auto_test(test_FrequencyViewKeyboardModifiers.cpp HelloUtauEditor)
helloutau_add_auto_test(test_KeymapFile.cpp HelloUtauEditor QActionKit::Core)
# The editor of the test registers the formats dio, frc, mrq and pmk, as the SynthToolSupport
# plugin does, whose library is in its own directory of the plugins.
helloutau_add_auto_test(test_Editor.cpp HelloUtauTestingEditor SynthToolSupportPlugin)
set_property(TEST test_Editor APPEND PROPERTY ENVIRONMENT_MODIFICATION
    "PATH=path_list_prepend:$<SHELL_PATH:$<TARGET_FILE_DIR:SynthToolSupportPlugin>>"
)
helloutau_add_auto_test(test_OtoWaveformKeyboardModifiers.cpp HelloUtauEditor)
helloutau_add_auto_test(test_OtoWaveformView.cpp HelloUtauEditor)
helloutau_add_auto_test(test_PianoRoll.cpp HelloUtauEditor)
helloutau_add_auto_test(test_Playback.cpp HelloUtauEditor)
helloutau_add_auto_test(test_SamplePreview.cpp HelloUtauEditor)
helloutau_add_auto_test(test_SoftwareBuildChannel.cpp HelloUtauEditor)
helloutau_add_auto_test(test_SynthToolTrust.cpp HelloUtauEditor)
helloutau_add_auto_test(test_UpdateChecker.cpp HelloUtauEditor)
helloutau_add_auto_test(test_UtauFolderSearch.cpp HelloUtauEditor)

# The tests write their archives with libzip, and give the update the updater of the build.
# test_SoftwareTreeUpdate writes its disk images with hdiutil, which stdc::Popen runs.
helloutau_add_auto_test(test_SoftwareTreeUpdate.cpp HelloUtauEditor libzip::zip
    stdcorelib::stdcorelib PROGRAMS hello-updater)
helloutau_add_auto_test(test_UpdateTask.cpp HelloUtauEditor libzip::zip PROGRAMS hello-updater)
helloutau_add_auto_test(test_UpdateTaskView.cpp HelloUtauEditor)

add_subdirectory(Dialogs)

# The contribution of the test, with a manifest of its own for each kind of window
qak_add_action_extension(_contribution_src TestContribution.xml
    FUNCTION testContribution
    NAMESPACE hello::daw::test
)
qak_add_action_extension(_bank_contribution_src TestBankContribution.xml
    FUNCTION testBankContribution
    NAMESPACE hello::daw::test
)
helloutau_add_auto_test(test_ActionContribution.cpp HelloUtauEditor QActionKit::Widgets)
target_sources(test_ActionContribution PRIVATE ${_contribution_src} ${_bank_contribution_src})
target_include_directories(test_ActionContribution PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# The plugins that test_AppLoader copies into plugin directories of its own, built with the
# interface ID of the native plugins: one that records its calls, and one whose initialization
# fails.
# As for the plugins of the application, the libraries they link are loaded already, and the
# applocal deployment of vcpkg is turned off for them, see helloutau/plugins.
set(VCPKG_APPLOCAL_DEPS OFF)
foreach(_plugin test_AppLoaderPlugin test_AppLoaderFailingPlugin)
    add_library(${_plugin} MODULE plugins/test_AppLoaderPlugin.cpp)
    target_link_libraries(${_plugin} PRIVATE stdcorelib::plugin HelloUtauEditor)
    stdc_add_plugin_metadata(TARGET ${_plugin} IID ${HELLOUTAU_PLUGIN_IID})
endforeach()
unset(VCPKG_APPLOCAL_DEPS)
target_compile_definitions(test_AppLoaderFailingPlugin PRIVATE TEST_APPLOADER_FAILS)

helloutau_add_auto_test(test_AppLoader.cpp HelloUtauEditor)
add_dependencies(test_AppLoader test_AppLoaderPlugin test_AppLoaderFailingPlugin CorePlugin
    ClassicPluginHostPlugin SynthToolSupportPlugin InterchangePlugin)
target_compile_definitions(test_AppLoader PRIVATE
    TEST_APPLOADER_CORE="$<TARGET_FILE:test_AppLoaderPlugin>"
    TEST_APPLOADER_FAILING_CORE="$<TARGET_FILE:test_AppLoaderFailingPlugin>"
    TEST_BUILTIN_PLUGINS_DIR="${HELLOUTAU_BUILD_PLUGINS_DIR}"
    TEST_TRANSLATIONS_DIR="${HELLOUTAU_BUILD_DATA_DIR}/translations"
)
