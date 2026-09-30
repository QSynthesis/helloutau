# The library-level content of Interchange: the joining of imported notes to a project, and the
# answers of the wizards given to the drivers.
add_auto_test(test_ImportMerge.cpp InterchangePlugin HelloKitEdit)
add_auto_test(test_PresetSelector.cpp InterchangePlugin HelloKitInterchange)

# The plugin lies in its own directory of the plugins, not beside the tests.
foreach(_test test_ImportMerge test_PresetSelector)
    set_property(TEST ${_test} APPEND PROPERTY ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<SHELL_PATH:$<TARGET_FILE_DIR:InterchangePlugin>>"
    )
endforeach()
