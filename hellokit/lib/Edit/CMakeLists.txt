project(HelloKitEdit
    VERSION ${HELLOKIT_VERSION}
    LANGUAGES CXX
)

file(GLOB_RECURSE _src *.cpp *.h ../../include/hellokit/Edit/*.h)

qm_find_qt(Core)

# substate is private: no public header of this library refers to an ss:: type.
hellokit_add_library(${PROJECT_NAME} SHARED
    QT_AUTOGEN
    SOURCES ${_src}
    LINKS HelloKitDocument
    LINKS_PRIVATE substate::substate substate::qsubstate
    QT_LINKS Core
    INCLUDE_PRIVATE ../../include/hellokit/Edit/** **
    MACRO_PREFIX HELLOKIT_EDIT
)
