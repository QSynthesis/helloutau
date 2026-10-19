set(_private substate::substate substate::qsubstate)
set(_private_include ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/Edit)

hellokit_add_auto_test(test_ProjectFields.cpp HelloKitEdit ${_private})
target_include_directories(test_ProjectFields PRIVATE ${_private_include})

hellokit_add_auto_test(test_ProjectTree.cpp HelloKitEdit ${_private})
target_include_directories(test_ProjectTree PRIVATE ${_private_include})

hellokit_add_auto_test(test_ProjectCommands.cpp HelloKitEdit)
hellokit_add_auto_test(test_ProjectDocument.cpp HelloKitEdit)
hellokit_add_auto_test(test_ProjectEdits.cpp HelloKitEdit)
hellokit_add_auto_test(test_ProjectRefs.cpp HelloKitEdit)
hellokit_add_auto_test(test_ProjectSession.cpp HelloKitEdit)
hellokit_add_auto_test(test_TrackTimeline.cpp HelloKitEdit)

hellokit_add_auto_test(test_VoiceBankTree.cpp HelloKitEdit ${_private})
target_include_directories(test_VoiceBankTree PRIVATE ${_private_include})

hellokit_add_auto_test(test_VoiceBankFields.cpp HelloKitEdit ${_private})
target_include_directories(test_VoiceBankFields PRIVATE ${_private_include})

hellokit_add_auto_test(test_VoiceBankSession.cpp HelloKitEdit ${_private})
target_include_directories(test_VoiceBankSession PRIVATE ${_private_include})

hellokit_add_auto_test(test_VoiceBankRefs.cpp HelloKitEdit)

hellokit_add_auto_test(test_VoiceBankEdits.cpp HelloKitEdit)
hellokit_add_auto_test(test_VoiceBankCommands.cpp HelloKitEdit)
hellokit_add_auto_test(test_VoiceBankSession_Disk.cpp HelloKitEdit)
hellokit_add_auto_test(test_VoiceBankDocument.cpp HelloKitEdit)
