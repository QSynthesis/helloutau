# One directory per module, mirroring include/hellokit. Each test file is named for the header it
# covers, so that what is tested and what is not can be seen from the directory listing.
qm_find_qt(Core)

add_subdirectory(Support)
add_subdirectory(Document)
add_subdirectory(VoiceBank)
add_subdirectory(Interchange)
