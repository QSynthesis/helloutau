set(_private substate::substate substate::qsubstate)
set(_private_include ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/Edit)

add_auto_test(test_ChangeLog.cpp HelloKitEdit ${_private})
target_include_directories(test_ChangeLog PRIVATE ${_private_include})

add_auto_test(test_EditSession.cpp HelloKitEdit ${_private})
target_include_directories(test_EditSession PRIVATE ${_private_include})

add_auto_test(test_NodeCommands.cpp HelloKitEdit ${_private})
target_include_directories(test_NodeCommands PRIVATE ${_private_include})

add_auto_test(test_NodeRef.cpp HelloKitEdit ${_private})
target_include_directories(test_NodeRef PRIVATE ${_private_include})

add_auto_test(test_ProjectFields.cpp HelloKitEdit ${_private})
target_include_directories(test_ProjectFields PRIVATE ${_private_include})

add_auto_test(test_ProjectTree.cpp HelloKitEdit ${_private})
target_include_directories(test_ProjectTree PRIVATE ${_private_include})

add_auto_test(test_CommandSyntax.cpp HelloKitEdit)
add_auto_test(test_ProjectCommands.cpp HelloKitEdit)
add_auto_test(test_ProjectEdits.cpp HelloKitEdit)
add_auto_test(test_ProjectRefs.cpp HelloKitEdit)
add_auto_test(test_ProjectSession.cpp HelloKitEdit)
add_auto_test(test_Slot.cpp HelloKitEdit)
