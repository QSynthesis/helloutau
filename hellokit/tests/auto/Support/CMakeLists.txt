add_auto_test(test_BinaryStream.cpp HelloKitSupport)
add_auto_test(test_FileName.cpp HelloKitSupport)
add_auto_test(test_JsonInterop.cpp HelloKitSupport)
add_auto_test(test_SettingsFile.cpp HelloKitSupport)
add_auto_test(test_TextCodec.cpp HelloKitSupport)
add_auto_test(test_TextSearch.cpp HelloKitSupport)
add_auto_test(test_RegistryInstantiator.cpp HelloKitSupport)
add_auto_test(test_OrderedObjectRegistryKeeper.cpp HelloKitSupport)
add_auto_test(test_ExitLauncher.cpp HelloKitSupport)
add_auto_test(test_TemporaryStorage.cpp HelloKitSupport)
add_auto_test(test_TranslationLoader.cpp HelloKitSupport)

# The test writes its archives with libzip, including archives that a conforming archiver never
# writes.
add_auto_test(test_ZipExtractor.cpp HelloKitSupport libzip::zip)

# The watcher starts the monitor program from the directory of the running program, which for a
# test is the directory of the test, see add_auto_test_program().
add_auto_test(test_FileSystemWatcher.cpp HelloKitSupport)
add_auto_test_program(test_FileSystemWatcher hello-fswatcher)
