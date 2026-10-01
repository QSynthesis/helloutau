# The library-level content of Core: the pages of the settings that the plugin adds to the
# catalog of the editor.
add_auto_test(test_CoreSettingPages.cpp CorePlugin HelloUtauEditor)
add_auto_test(test_KeymapSettingPage.cpp CorePlugin HelloUtauEditor)
add_auto_test(test_MenusSettingPage.cpp CorePlugin HelloUtauEditor)
add_auto_test(test_PluginSettingPage.cpp CorePlugin HelloUtauEditor)

# The plugin is in its own directory of the plugins, not beside the tests.
foreach(_test test_CoreSettingPages test_KeymapSettingPage test_MenusSettingPage
        test_PluginSettingPage)
    set_property(TEST ${_test} APPEND PROPERTY ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<SHELL_PATH:$<TARGET_FILE_DIR:CorePlugin>>"
    )
endforeach()
