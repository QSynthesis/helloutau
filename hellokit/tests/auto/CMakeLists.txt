# One directory per module, mirroring include/hellokit, and tools/ for the programs in
# hellokit/tools. Each test file is named after the unit it covers, so that the directory listing
# shows which units are tested.
qm_find_qt(Core)

# hellokit_add_auto_test(<source> [link libraries...]
#     [RESOURCES <files or directories...>]
#     [RESOURCE_MACRO <macro>]
#     [PROGRAMS <targets...>]
# )
#
# Adds the test of \a source as a test executable of hellokit, see add_auto_test().
function(hellokit_add_auto_test)
    add_auto_test(hellokit_add_executable ${ARGN})
endfunction()

add_subdirectory(Support)
add_subdirectory(AudioFile)
add_subdirectory(Document)
add_subdirectory(EditBase)
add_subdirectory(Edit)
add_subdirectory(VoiceBank)
add_subdirectory(Synth)
add_subdirectory(Interchange)
add_subdirectory(Update)
add_subdirectory(NativeSystem)
add_subdirectory(tools)
