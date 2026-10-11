# The library-level content of FrequencyEditor: the diagnoser of moresampler.
add_auto_test(test_MoresamplerDiagnoser.cpp FrequencyEditorPlugin)

# The plugin is in its own directory of the plugins, not beside the tests.
set_property(TEST test_MoresamplerDiagnoser APPEND PROPERTY ENVIRONMENT_MODIFICATION
    "PATH=path_list_prepend:$<SHELL_PATH:$<TARGET_FILE_DIR:FrequencyEditorPlugin>>"
)
