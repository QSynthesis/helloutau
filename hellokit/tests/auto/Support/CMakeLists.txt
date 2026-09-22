add_auto_test(test_TextCodec.cpp HelloKitSupport)

# The watcher starts the helper from beside itself, which for a test is out/bin, where the
# helper is built. It has to be there before the test runs.
add_auto_test(test_FileSystemWatcher.cpp HelloKitSupport)
add_dependencies(test_FileSystemWatcher hello-fswatcher)
