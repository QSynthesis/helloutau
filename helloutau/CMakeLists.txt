project(helloutau
    VERSION ${HELLO_VERSION}
    LANGUAGES CXX
    DESCRIPTION "HelloUtau Editor Libraries"
)

# ----------------------------------
# Project Variables
# ----------------------------------
set(HELLOUTAU_INSTALL ${HELLO_INSTALL})
set(HELLOUTAU_VERSION ${PROJECT_VERSION})
set(HELLOUTAU_INSTALL_NAME ${PROJECT_NAME})

include("conf.cmake")

helloutau_init_buildsystem(HELLOUTAU)

add_subdirectory(lib)

add_subdirectory(plugins)

add_subdirectory(tools)

helloutau_install()

helloutau_finish_buildsystem()
