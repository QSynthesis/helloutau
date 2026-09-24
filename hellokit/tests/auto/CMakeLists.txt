# One directory per module, mirroring include/hellokit, and tools/ for the programs in
# hellokit/tools. Each test file is named after the unit it covers, so that the directory listing
# shows which units are tested.
qm_find_qt(Core)

add_subdirectory(Support)
add_subdirectory(Document)
add_subdirectory(Edit)
add_subdirectory(VoiceBank)
add_subdirectory(Synth)
add_subdirectory(Interchange)
add_subdirectory(tools)
