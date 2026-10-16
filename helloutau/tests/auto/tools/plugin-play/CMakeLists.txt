# Test of hello-plugin-play, which runs the program beside a stand-in for the host: a job object
# and a named pipe on Windows, a process group and a local socket elsewhere. Written in Python, as
# the tests of the other programs, because the test checks the message at the endpoint, the exit
# status and the record of a stand-in for the original program, which a script expresses more
# directly than a test that starts the process.
#
# Set Python3_EXECUTABLE if the first Python found is unsuitable, which on Windows may be the
# Microsoft Store placeholder on PATH.
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# The stand-in for the original playinplugin. On Windows a GUI program as the original, so that no
# console window appears. The test copies it into a folder of its own, and it therefore links the
# C++ runtime statically.
add_executable(plugin-play-original-stub original_stub.cpp)
target_compile_features(plugin-play-original-stub PRIVATE cxx_std_17)
target_link_libraries(plugin-play-original-stub PRIVATE HelloKitNativeSystem)
if(WIN32)
    set_target_properties(plugin-play-original-stub PROPERTIES
        WIN32_EXECUTABLE TRUE
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    if(MINGW)
        target_compile_options(plugin-play-original-stub PRIVATE -municode)
        target_link_options(plugin-play-original-stub PRIVATE -municode)
    else()
        target_link_options(plugin-play-original-stub PRIVATE "LINKER:/ENTRY:wmainCRTStartup")
    endif()
endif()

add_test(NAME test_plugin_play
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/test_plugin_play.py
        $<TARGET_FILE:hello-plugin-play> $<TARGET_FILE:plugin-play-original-stub> ${HELLO_VERSION}
)
set_tests_properties(test_plugin_play PROPERTIES TIMEOUT 180)
