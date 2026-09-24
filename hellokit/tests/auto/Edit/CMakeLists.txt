add_auto_test(test_EditSession.cpp HelloKitEdit)
add_auto_test(test_NodeRef.cpp HelloKitEdit)
add_auto_test(test_ProjectRefs.cpp HelloKitEdit)

# Tests the private conversion between a project and its tree, which the library exports for this
# purpose, and therefore uses substate directly.
add_auto_test(test_ProjectTree.cpp HelloKitEdit substate::substate substate::qsubstate)
target_include_directories(test_ProjectTree PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/Edit)
