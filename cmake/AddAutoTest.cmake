# QtTest based automatic tests.
#
# One executable per test source rather than a single binary containing every case. A crash, a
# hang or a static initialization failure is then confined to the class under test instead of
# aborting the entire suite, and \c ctest can run the cases in parallel.

qm_find_qt(Test)

# add_auto_test(<source> [extra link libraries...]
#     [RESOURCES <files or directories...>]
#     [RESOURCE_MACRO <macro>]
# )
#
# The target and the \c ctest case both take their name from \a source, so \c test_Foo.cpp yields
# \c test_Foo. The source declares one QObject whose private slots are the cases, and ends with
# \c QTEST_APPLESS_MAIN and the include of its own \c .moc file.
#
# Resources are copied to <target-file-directory>/<target-name>_data. The target receives that
# directory through the \c TEST_RESOURCE_DIRECTORY compile definition. \c RESOURCE_MACRO replaces
# the default definition name when supplied.
function(add_auto_test _src)
    set(options)
    set(oneValueArgs RESOURCE_MACRO)
    set(multiValueArgs RESOURCES)
    cmake_parse_arguments(FUNC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    get_filename_component(_name ${_src} NAME_WE)
    add_executable(${_name} ${_src})

    # The test class is declared in the source file, so moc must process it.
    set_target_properties(${_name} PROPERTIES AUTOMOC ON)

    target_link_libraries(${_name} PRIVATE Qt${QT_VERSION_MAJOR}::Test
        ${FUNC_UNPARSED_ARGUMENTS})

    if(FUNC_RESOURCES)
        set(_resource_macro TEST_RESOURCE_DIRECTORY)
        if(FUNC_RESOURCE_MACRO)
            set(_resource_macro ${FUNC_RESOURCE_MACRO})
        endif()

        set(_resource_directory ${_name}_data)
        qm_add_copy_command(${_name}
            SOURCES ${FUNC_RESOURCES}
            DESTINATION ${_resource_directory}
            SKIP_INSTALL
        )
        target_compile_definitions(${_name} PRIVATE
            ${_resource_macro}="$<TARGET_FILE_DIR:${_name}>/${_resource_directory}"
        )
    endif()

    add_test(NAME ${_name} COMMAND $<TARGET_FILE:${_name}>)

    # Windows searches for a DLL beside the executable and then along PATH, and in a build tree
    # Qt is in neither location. Without this, a test that uses Qt terminates at load time with
    # 0xc0000135 and no message, which appears as a crash rather than as a missing library.
    if(WIN32 AND TARGET Qt${QT_VERSION_MAJOR}::Core)
        set_tests_properties(${_name} PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<SHELL_PATH:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>>"
        )
    endif()
endfunction()
