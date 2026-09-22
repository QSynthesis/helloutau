# One directory per module, mirroring include/hellokit, and tools/ for the programs in
# hellokit/tools. Each test file is named for what it covers, so that what is tested and what is
# not can be seen from the directory listing.
qm_find_qt(Core)

add_subdirectory(Support)
add_subdirectory(Document)
add_subdirectory(VoiceBank)
add_subdirectory(Synth)
add_subdirectory(Interchange)
add_subdirectory(tools)
