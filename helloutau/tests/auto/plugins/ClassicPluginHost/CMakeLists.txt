# The library-level content of ClassicPluginHost: the discovery of the plugins of UTAU, and the
# exchange of the temporary file with them.
add_auto_test(test_ClassicPlugin.cpp ClassicPluginHostPlugin)
add_auto_test(test_ClassicPluginExchange.cpp ClassicPluginHostPlugin HelloKitEdit stdutau::stdutau)

# The plugin lies in its own directory of the plugins, not beside the tests.
foreach(_test test_ClassicPlugin test_ClassicPluginExchange)
    set_property(TEST ${_test} APPEND PROPERTY ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<SHELL_PATH:$<TARGET_FILE_DIR:ClassicPluginHostPlugin>>"
    )
endforeach()
