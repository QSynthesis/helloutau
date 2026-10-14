# Test of hello-plugin-play, which runs the program in a job object beside a named pipe that stands
# in for the host. Written in Python, as the tests of the other programs, because the test checks
# the message in the pipe, the exit status and the record of a stand-in for the original program,
# which a script expresses more directly than a test that starts the process.
#
# Set Python3_EXECUTABLE if the first Python found is unsuitable, which on Windows may be the
# Microsoft Store placeholder on PATH.
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# The stand-in for the original playinplugin.exe. A GUI program as the original, so that no
# console window appears. The test copies it into a folder of its own, and it therefore links the
# C++ runtime statically.
add_executable(plugin-play-original-stub WIN32 original_stub.cpp)
target_compile_features(plugin-play-original-stub PRIVATE cxx_std_17)
set_target_properties(plugin-play-original-stub PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)
if(MINGW)
    target_compile_options(plugin-play-original-stub PRIVATE -municode)
    target_link_options(plugin-play-original-stub PRIVATE -municode)
else()
    target_link_options(plugin-play-original-stub PRIVATE "LINKER:/ENTRY:wmainCRTStartup")
endif()

add_test(NAME test_plugin_play
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/test_plugin_play.py
        $<TARGET_FILE:hello-plugin-play> $<TARGET_FILE:plugin-play-original-stub>
)
set_tests_properties(test_plugin_play PROPERTIES TIMEOUT 180)
