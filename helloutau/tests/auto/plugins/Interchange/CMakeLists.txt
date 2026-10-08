# The library-level content of Interchange: the insertion of imported notes into a project, the
# selector with the requests of the wizards, the service, and the decoding of the
# text of an inspected file.
add_auto_test(test_ImportMerge.cpp InterchangePlugin HelloKitEdit)
add_auto_test(test_PresetSelector.cpp InterchangePlugin HelloKitInterchange)
add_auto_test(test_InterchangeService.cpp InterchangePlugin)
add_auto_test(test_SourcePreview.cpp InterchangePlugin HelloKitSupport)

# The plugin is in its own directory of the plugins, not beside the tests.
foreach(_test test_ImportMerge test_PresetSelector test_InterchangeService
        test_SourcePreview)
    set_property(TEST ${_test} APPEND PROPERTY ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<SHELL_PATH:$<TARGET_FILE_DIR:InterchangePlugin>>"
    )
endforeach()
