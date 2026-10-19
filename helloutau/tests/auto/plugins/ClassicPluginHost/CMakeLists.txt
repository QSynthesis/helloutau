# The library-level content of ClassicPluginHost: the discovery of the plugins of UTAU, the
# exchange of the temporary file with them, the run of their programs, the records of the
# programs that the user has trusted, the pipe of their previews, and the installation of
# the preview program.
helloutau_add_auto_test(test_ClassicPlugin.cpp ClassicPluginHostPlugin)
helloutau_add_auto_test(test_ClassicPluginTrust.cpp ClassicPluginHostPlugin)
helloutau_add_auto_test(test_ClassicPluginExchange.cpp ClassicPluginHostPlugin HelloKitEdit stdutau::stdutau)
helloutau_add_auto_test(test_ClassicPluginRunner.cpp ClassicPluginHostPlugin)
helloutau_add_auto_test(test_ClassicPluginPreviewInstallation.cpp ClassicPluginHostPlugin)

# The test connects to the pipe as hello-plugin-play does.
qm_find_qt(Network)
helloutau_add_auto_test(test_ClassicPluginPreviewServer.cpp ClassicPluginHostPlugin
    Qt${QT_VERSION_MAJOR}::Network)

# The plugin is located in its own directory under the plugins directory, not beside the tests.
foreach(_test test_ClassicPlugin test_ClassicPluginTrust test_ClassicPluginExchange
        test_ClassicPluginRunner test_ClassicPluginPreviewServer
        test_ClassicPluginPreviewInstallation)
    set_property(TEST ${_test} APPEND PROPERTY ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<SHELL_PATH:$<TARGET_FILE_DIR:ClassicPluginHostPlugin>>"
    )
endforeach()
