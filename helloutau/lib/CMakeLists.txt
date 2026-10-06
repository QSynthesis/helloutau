add_subdirectory(Theme)
add_subdirectory(Widgets)
add_subdirectory(Audio)
add_subdirectory(Editor)

if(HELLO_BUILD_TESTS OR HELLO_INSTALL_TESTING)
    add_subdirectory(Testing)
endif()
