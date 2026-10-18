# The library-level content of Core: the pages of the settings that the plugin adds to the
# catalog of the editor.
add_auto_test(test_CoreSettingPages.cpp CorePlugin HelloUtauEditor)
add_auto_test(test_KeyboardModifierSettingPage.cpp CorePlugin HelloUtauEditor)
add_auto_test(test_KeymapSettingPage.cpp CorePlugin HelloUtauTestingEditor)
add_auto_test(test_MenusSettingPage.cpp CorePlugin HelloUtauTestingEditor)
add_auto_test(test_PluginSettingPage.cpp CorePlugin HelloUtauEditor)
# The built plugins, in the bundle on macOS, where AppLoader::builtinPluginPath() of a test
# beside the bundle finds nothing
target_compile_definitions(test_PluginSettingPage PRIVATE
    TEST_BUILTIN_PLUGINS_DIR="${HELLOUTAU_BUILD_PLUGINS_DIR}"
)

# The plugin is in its own directory of the plugins, not beside the tests.
foreach(_test test_CoreSettingPages test_KeyboardModifierSettingPage test_KeymapSettingPage
        test_MenusSettingPage test_PluginSettingPage)
    set_property(TEST ${_test} APPEND PROPERTY ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<SHELL_PATH:$<TARGET_FILE_DIR:CorePlugin>>"
    )
endforeach()
