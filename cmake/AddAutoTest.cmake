# QtTest based automatic tests.
#
# One executable per test source rather than a single binary containing every case. A crash, a
# hang or a static initialization failure is then confined to the class under test instead of
# aborting the entire suite, and \c ctest can run the cases in parallel.

qm_find_qt(Test)

# configure_auto_test(<target>
#     [RESOURCES <files or directories...>]
#     [RESOURCE_MACRO <macro>]
#     [PROGRAMS <targets...>]
# )
#
# Makes the executable \a target a test with the \c ctest case of the same name. A module creates
# the target with its own <proj>_add_executable(), see add_auto_test(), so that the tests are
# built as its programs are. The source declares one QObject whose private slots are the cases,
# and ends with \c QTEST_APPLESS_MAIN and the include of its own \c .moc file.
#
# Resources are copied to <target-file-directory>/<target-name>_data. The target receives that
# directory through the \c TEST_RESOURCE_DIRECTORY compile definition. \c RESOURCE_MACRO replaces
# the default definition name when supplied.
#
# \c PROGRAMS names the programs that the code under test starts. Each is built before the test,
# and the target receives its path through the compile definition \c TEST_PROGRAM_<NAME>, the
# target name in upper case with each other character than a letter or a digit replaced by an
# underscore: \c hello-fswatcher yields \c TEST_PROGRAM_HELLO_FSWATCHER. The test passes the path
# to the code under test rather than relying on the directory of the running program, because
# the tests are built elsewhere than the programs on macOS, beside the bundle.
function(configure_auto_test _target)
    set(options)
    set(oneValueArgs RESOURCE_MACRO)
    set(multiValueArgs RESOURCES PROGRAMS)
    cmake_parse_arguments(FUNC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set_target_properties(${_target} PROPERTIES AUTOMOC ON)

    target_link_libraries(${_target} PRIVATE Qt${QT_VERSION_MAJOR}::Test)

    if(FUNC_RESOURCES)
        set(_resource_macro TEST_RESOURCE_DIRECTORY)
        if(FUNC_RESOURCE_MACRO)
            set(_resource_macro ${FUNC_RESOURCE_MACRO})
        endif()

        set(_resource_directory ${_target}_data)
        qm_add_copy_command(${_target}
            SOURCES ${FUNC_RESOURCES}
            DESTINATION ${_resource_directory}
            SKIP_INSTALL
        )
        target_compile_definitions(${_target} PRIVATE
            ${_resource_macro}="$<TARGET_FILE_DIR:${_target}>/${_resource_directory}"
        )
    endif()

    foreach(_program IN LISTS FUNC_PROGRAMS)
        add_dependencies(${_target} ${_program})
        string(TOUPPER "${_program}" _macro)
        string(MAKE_C_IDENTIFIER "${_macro}" _macro)
        target_compile_definitions(${_target} PRIVATE
            TEST_PROGRAM_${_macro}="$<TARGET_FILE:${_program}>"
        )
    endforeach()

    add_test(NAME ${_target} COMMAND $<TARGET_FILE:${_target}>)

    # Windows searches for a DLL beside the executable and then along PATH, and in a build tree
    # Qt is in neither location. Without this, a test that uses Qt terminates at load time with
    # 0xc0000135 and no message, which appears as a crash rather than as a missing library.
    if(WIN32 AND TARGET Qt${QT_VERSION_MAJOR}::Core)
        set_tests_properties(${_target} PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<SHELL_PATH:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>>"
        )
    endif()

    # On Windows, QtTest writes its results with OutputDebugString() instead of to the standard
    # output if the process has no console window, as on the runners of CI (qplaintestlogger.cpp
    # of Qt 6.11). The results of a failed test would then be missing from the output of ctest.
    if(WIN32)
        set_property(TEST ${_target} APPEND PROPERTY ENVIRONMENT_MODIFICATION
            "QT_FORCE_STDERR_LOGGING=set:1"
        )
    endif()
endfunction()

# add_auto_test(<function> <source> [link libraries...]
#     [RESOURCES <files or directories...>]
#     [RESOURCE_MACRO <macro>]
#     [PROGRAMS <targets...>]
# )
#
# Creates the test of \a source with \a function, the <proj>_add_executable() of a module, as a
# test executable of the module: built into the test directory of the build, out/bin, and not
# installed. The target and the \c ctest case take their name from \a source, so \c test_Foo.cpp
# yields \c test_Foo. The remaining arguments are those of configure_auto_test().
function(add_auto_test _function _src)
    set(options)
    set(oneValueArgs RESOURCE_MACRO)
    set(multiValueArgs RESOURCES PROGRAMS)
    cmake_parse_arguments(FUNC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    get_filename_component(_name ${_src} NAME_WE)
    cmake_language(CALL ${_function} ${_name} TEST CONSOLE
        SOURCES ${_src}
        LINKS ${FUNC_UNPARSED_ARGUMENTS}
    )

    set(_args)
    if(FUNC_RESOURCES)
        list(APPEND _args RESOURCES ${FUNC_RESOURCES})
    endif()
    if(FUNC_RESOURCE_MACRO)
        list(APPEND _args RESOURCE_MACRO ${FUNC_RESOURCE_MACRO})
    endif()
    if(FUNC_PROGRAMS)
        list(APPEND _args PROGRAMS ${FUNC_PROGRAMS})
    endif()
    configure_auto_test(${_name} ${_args})
endfunction()
