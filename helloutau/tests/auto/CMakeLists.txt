# One directory per library, mirroring include/helloutau, as in hellokit/tests/auto.
qm_find_qt(Core)

# helloutau_add_auto_test(<source> [link libraries...]
#     [RESOURCES <files or directories...>]
#     [RESOURCE_MACRO <macro>]
#     [PROGRAMS <targets...>]
# )
#
# Adds the test of \a source as a test executable of helloutau, see add_auto_test().
function(helloutau_add_auto_test)
    add_auto_test(helloutau_add_executable ${ARGN})
endfunction()

add_subdirectory(Theme)
add_subdirectory(Widgets)
add_subdirectory(Audio)
add_subdirectory(Editor)
add_subdirectory(Testing)
add_subdirectory(plugins)
add_subdirectory(tools)
