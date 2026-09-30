# The library-level content of ClassicPluginHost: the discovery of the plugins of UTAU, the
# exchange of the temporary file with them, and the run of their programs.
add_auto_test(test_ClassicPlugin.cpp ClassicPluginHostPlugin)
add_auto_test(test_ClassicPluginExchange.cpp ClassicPluginHostPlugin HelloKitEdit stdutau::stdutau)
add_auto_test(test_ClassicPluginRunner.cpp ClassicPluginHostPlugin)

# The plugin is located in its own directory under the plugins directory, not beside the tests.
foreach(_test test_ClassicPlugin test_ClassicPluginExchange test_ClassicPluginRunner)
    set_property(TEST ${_test} APPEND PROPERTY ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<SHELL_PATH:$<TARGET_FILE_DIR:ClassicPluginHostPlugin>>"
    )
endforeach()
