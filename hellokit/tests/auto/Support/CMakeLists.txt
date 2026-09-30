add_auto_test(test_TextCodec.cpp HelloKitSupport)
add_auto_test(test_TextSearch.cpp HelloKitSupport)

# The watcher starts the monitor program from its own directory, which for a test is out/bin,
# where the program is built. The program must be built before the test runs.
add_auto_test(test_FileSystemWatcher.cpp HelloKitSupport)
add_dependencies(test_FileSystemWatcher hello-fswatcher)
