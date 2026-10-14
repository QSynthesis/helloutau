# Test of hello-updater, which runs the program on temporary directories. Written in Python,
# because the test checks directories, exit statuses and a log, which a script expresses more
# directly than a test that starts the process.
find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_test(NAME test_updater
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/test_updater.py
        $<TARGET_FILE:hello-updater> ${HELLO_VERSION}
)
set_tests_properties(test_updater PROPERTIES TIMEOUT 180)
