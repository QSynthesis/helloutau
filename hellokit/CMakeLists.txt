project(hellokit
    VERSION ${HELLO_VERSION}
    LANGUAGES CXX
    DESCRIPTION "HelloUTAU core libraries"
)

# ----------------------------------
# Project Variables
# ----------------------------------
set(HELLOKIT_INSTALL ${HELLO_INSTALL})
set(HELLOKIT_VERSION ${PROJECT_VERSION})
set(HELLOKIT_INSTALL_NAME ${PROJECT_NAME})

include("conf.cmake")

hellokit_init_buildsystem(HELLOKIT)

add_subdirectory(lib)

# Programs the libraries start, which ship with them.
add_subdirectory(tools)

if(HELLO_BUILD_TESTS)
    add_subdirectory(tests)
endif()

hellokit_install()

hellokit_finish_buildsystem()
