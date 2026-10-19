project(hellokit
    VERSION ${HELLO_VERSION}
    LANGUAGES CXX
    DESCRIPTION "HelloUtau Core Libraries"
)

# ----------------------------------
# Project Variables
# ----------------------------------
set(HELLOKIT_INSTALL ${HELLO_INSTALL})
set(HELLOKIT_VERSION ${PROJECT_VERSION})
set(HELLOKIT_INSTALL_NAME ${PROJECT_NAME})

include("conf.cmake")

# The libraries and the programs of hellokit go into the bundle of the application on macOS.
set(HELLOKIT_MACOSX_BUNDLE_NAME ${HELLO_MACOSX_BUNDLE_NAME})

hellokit_init_buildsystem(HELLOKIT)

# The definitions of hellokit/Config.h, which hellokit_finish_buildsystem() generates: the version
# of the repository, which the application and the programs show, the triplet of the build, by
# which the application selects the assets of a release, and the channel of the build
qm_add_definition(HELLOKIT_VERSION ${PROJECT_VERSION} STRING_LITERAL)
qm_add_definition(HELLOKIT_TRIPLET ${HELLO_TRIPLET} STRING_LITERAL)
qm_add_definition(HELLOKIT_DEVELOPER "Team OpenVPI" STRING_LITERAL)
qm_add_definition(HELLOKIT_BUILD_CHANNEL ${HELLO_BUILD_CHANNEL} STRING_LITERAL)

add_subdirectory(lib)

# Programs started by the libraries and distributed with them.
add_subdirectory(tools)

if(HELLO_BUILD_TESTS)
    add_subdirectory(tests)
endif()

hellokit_install()

hellokit_finish_buildsystem()
