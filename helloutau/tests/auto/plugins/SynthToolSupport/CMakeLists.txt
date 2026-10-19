# The library-level content of SynthToolSupport: the formats dio, frc, mrq and pmk, and the
# diagnoser of moresampler.
foreach(_test test_DioFormat test_FrcFormat test_MrqFormat test_PmkFormat
              test_MoresamplerDiagnoser)
    helloutau_add_auto_test(${_test}.cpp SynthToolSupportPlugin)
    # The plugin is in its own directory of the plugins, not beside the tests.
    set_property(TEST ${_test} APPEND PROPERTY ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<SHELL_PATH:$<TARGET_FILE_DIR:SynthToolSupportPlugin>>"
    )
endforeach()
