set(_private substate::substate substate::qsubstate)

hellokit_add_auto_test(test_ChangeLog.cpp HelloKitEditBase ${_private})
hellokit_add_auto_test(test_EditSession.cpp HelloKitEditBase ${_private})
hellokit_add_auto_test(test_FieldTable.cpp HelloKitEditBase ${_private})
hellokit_add_auto_test(test_NodeCommands.cpp HelloKitEditBase ${_private})
hellokit_add_auto_test(test_NodeRef.cpp HelloKitEditBase ${_private})

hellokit_add_auto_test(test_CommandSyntax.cpp HelloKitEditBase)
hellokit_add_auto_test(test_Slot.cpp HelloKitEditBase)
