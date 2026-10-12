add_auto_test(test_ActionLayoutsFile.cpp HelloUtauEditor QActionKit::Core)
add_auto_test(test_AppSettings.cpp HelloUtauEditor)
add_auto_test(test_KeymapFile.cpp HelloUtauEditor QActionKit::Core)
# The editor of the test registers the formats dio and mrq of the FrequencyEditor plugin, as the
# plugin does, whose library is in its own directory of the plugins.
add_auto_test(test_Editor.cpp HelloUtauTestingEditor FrequencyEditorPlugin)
set_property(TEST test_Editor APPEND PROPERTY ENVIRONMENT_MODIFICATION
    "PATH=path_list_prepend:$<SHELL_PATH:$<TARGET_FILE_DIR:FrequencyEditorPlugin>>"
)
add_auto_test(test_OtoWaveformView.cpp HelloUtauEditor)
add_auto_test(test_PianoRoll.cpp HelloUtauEditor)
add_auto_test(test_Playback.cpp HelloUtauEditor)
add_auto_test(test_SamplePreview.cpp HelloUtauEditor)
add_auto_test(test_SynthToolTrust.cpp HelloUtauEditor)
add_auto_test(test_UpdateChecker.cpp HelloUtauEditor)

# The test writes its archives with libzip, and copies hello-updater from out/bin, where the
# program must be built before the test runs.
add_auto_test(test_PortableUpdate.cpp HelloUtauEditor libzip::zip)
add_dependencies(test_PortableUpdate hello-updater)
add_auto_test(test_UpdateTask.cpp HelloUtauEditor libzip::zip)
add_dependencies(test_UpdateTask hello-updater)
add_auto_test(test_UpdateTaskView.cpp HelloUtauEditor)

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
add_auto_test(test_ActionContribution.cpp HelloUtauEditor QActionKit::Widgets)
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

add_auto_test(test_AppLoader.cpp HelloUtauEditor)
add_dependencies(test_AppLoader test_AppLoaderPlugin test_AppLoaderFailingPlugin CorePlugin
    ClassicPluginHostPlugin FrequencyEditorPlugin InterchangePlugin)
target_compile_definitions(test_AppLoader PRIVATE
    TEST_APPLOADER_CORE="$<TARGET_FILE:test_AppLoaderPlugin>"
    TEST_APPLOADER_FAILING_CORE="$<TARGET_FILE:test_AppLoaderFailingPlugin>"
)
