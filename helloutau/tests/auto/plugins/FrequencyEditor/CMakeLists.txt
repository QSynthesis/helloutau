# The library-level content of FrequencyEditor: the formats dio and mrq, and the diagnoser of
# moresampler.
foreach(_test test_DioFormat test_MrqFormat test_MoresamplerDiagnoser)
    add_auto_test(${_test}.cpp FrequencyEditorPlugin)
    # The plugin is in its own directory of the plugins, not beside the tests.
    set_property(TEST ${_test} APPEND PROPERTY ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<SHELL_PATH:$<TARGET_FILE_DIR:FrequencyEditorPlugin>>"
    )
endforeach()
