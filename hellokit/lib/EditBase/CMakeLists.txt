project(HelloKitEditBase
    VERSION ${HELLO_VERSION}
    LANGUAGES CXX
)

file(GLOB_RECURSE _src *.cpp *.h ../../include/hellokit/EditBase/*.h)

qm_find_qt(Core)

hellokit_add_library(${PROJECT_NAME} SHARED
    QT_AUTOGEN
    SOURCES ${_src}
    LINKS HelloKitSupport
    LINKS_PRIVATE stdcorelib::stdcorelib substate::substate substate::qsubstate
    QT_LINKS Core
    INCLUDE_PRIVATE ../../include/hellokit/EditBase/** **
    MACRO_PREFIX HELLOKIT_EDITBASE
)
