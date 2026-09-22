add_auto_test(test_VoiceBankConfig.cpp HelloKitVoiceBank)
add_auto_test(test_VoiceBankSource.cpp HelloKitVoiceBank)
add_auto_test(test_VoiceBank.cpp HelloKitVoiceBank)
add_auto_test(test_VoiceBankSave.cpp HelloKitVoiceBank)
add_auto_test(test_VoiceBankCharset.cpp HelloKitVoiceBank)
add_auto_test(test_VoiceBankDiskChanges.cpp HelloKitVoiceBank)

# Starts the watcher, whose program has to be built and beside it.
add_auto_test(test_VoiceBankCheckScheduler.cpp HelloKitVoiceBank)
add_dependencies(test_VoiceBankCheckScheduler hello-fswatcher)
