# Protocol test of hello-fswatcher in isolation, over its pipes. Written in Python, because it
# verifies the exchanged text, which a script expresses more directly than a test that starts a
# process and parses its output manually. See test_fswatcher.py for the reason this layer is
# tested separately from FileSystemWatcher.
#
# Set Python3_EXECUTABLE if the first Python found is unsuitable, which on Windows may be the
# Microsoft Store placeholder on PATH.
find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_test(NAME test_fswatcher
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/test_fswatcher.py
        $<TARGET_FILE:hello-fswatcher>
)
set_tests_properties(test_fswatcher PROPERTIES TIMEOUT 180)
