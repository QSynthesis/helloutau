# hello-fswatcher on its own, spoken to over its pipes. In Python, since what it checks is the
# text going back and forth, and a script says that more plainly than a test that has to start
# a process and parse its output by hand. See test_fswatcher.py for why this layer is tested
# apart from FileSystemWatcher.
#
# Set Python3_EXECUTABLE where the Python found first is the wrong one, which on Windows can be
# the placeholder the Store puts on PATH.
find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_test(NAME test_fswatcher
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/test_fswatcher.py
        $<TARGET_FILE:hello-fswatcher>
)
set_tests_properties(test_fswatcher PROPERTIES TIMEOUT 180)
