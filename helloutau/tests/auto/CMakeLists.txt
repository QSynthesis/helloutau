# One directory per library, mirroring include/helloutau, as in hellokit/tests/auto.
qm_find_qt(Core)

add_subdirectory(Theme)
add_subdirectory(Widgets)
add_subdirectory(Audio)
add_subdirectory(Editor)
