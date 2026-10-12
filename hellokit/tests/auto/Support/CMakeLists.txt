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

# The watcher starts the monitor program from its own directory, which for a test is out/bin,
# where the program is built. The program must be built before the test runs.
add_auto_test(test_FileSystemWatcher.cpp HelloKitSupport)
add_dependencies(test_FileSystemWatcher hello-fswatcher)
