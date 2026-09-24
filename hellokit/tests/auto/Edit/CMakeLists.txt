# The tests of the generic session use a tree built through the extension interface of the
# library, and the tests of the conversion use the private conversion functions, which the library
# exports for this purpose. Both therefore use substate directly.
set(_private substate::substate substate::qsubstate)
set(_private_include ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/Edit)

add_auto_test(test_EditSession.cpp HelloKitEdit ${_private})
target_include_directories(test_EditSession PRIVATE ${_private_include})

add_auto_test(test_NodeRef.cpp HelloKitEdit ${_private})
target_include_directories(test_NodeRef PRIVATE ${_private_include})

add_auto_test(test_ProjectTree.cpp HelloKitEdit ${_private})
target_include_directories(test_ProjectTree PRIVATE ${_private_include})

add_auto_test(test_ProjectEdits.cpp HelloKitEdit)
add_auto_test(test_ProjectRefs.cpp HelloKitEdit)
add_auto_test(test_ProjectSession.cpp HelloKitEdit)
