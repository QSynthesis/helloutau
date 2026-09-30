# Native plugins bundled with the application. Each plugin has its own directory under
# ${HELLOUTAU_BUILD_PLUGINS_DIR}, following the directory layout of stdcorelib.plugin. See
# docs/Plugins.md.

# The application loads every library that a plugin links before it loads the plugin. The applocal
# deployment of vcpkg would instead copy the libraries of the vcpkg installed tree beside each
# plugin, including versions of stdcorelib other than the version that the application uses.
set(VCPKG_APPLOCAL_DEPS OFF)

# helloutau_add_native_plugin(<target>
#     ID <id> DISPLAY_NAME <name> CATEGORY <directory>
#     [DEPENDENCIES <id>[;<id>...]]
#     [DIRECTORY <plugins directory>]
#     [arguments of helloutau_add_plugin()...]
# )
#
# Builds the library into the subdirectory <directory> of the plugins directory, embeds the
# interface ID HELLOUTAU_PLUGIN_IID, and writes beside the library the plugin.json that the
# directory layout reads. The plugin.json records the library name without prefix and suffix, the
# ID, the display name, the version, and the plugins of DEPENDENCIES as required dependencies of
# the same version. DEPENDENCIES is a one-value keyword whose value is a list, because the
# remaining arguments are forwarded to helloutau_add_plugin() under keywords that this function
# does not parse.
#
# The plugin is installed with its headers and exported like a library, because other plugins may
# depend on it. Its public headers are located beside its sources in helloutau/plugins/<directory>
# and are included as <<directory>/<header>> in the build and after installation. The plugin
# class and the remaining implementation are located in Internal, which is not installed. If
# DIRECTORY is specified, as for the plugins of the tests, the plugin is built into that directory
# and not installed.
function(helloutau_add_native_plugin _target)
    set(options)
    set(oneValueArgs ID DISPLAY_NAME CATEGORY DIRECTORY DEPENDENCIES)
    set(multiValueArgs)
    cmake_parse_arguments(FUNC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(_plugins_dir ${HELLOUTAU_BUILD_PLUGINS_DIR})
    set(_extra_args)
    if(FUNC_DIRECTORY)
        set(_plugins_dir ${FUNC_DIRECTORY})
        # A generator expression, because helloutau_add_plugin() uses such a path unchanged
        # instead of placing it below its own directory
        set(_output "$<1:${FUNC_DIRECTORY}/${FUNC_CATEGORY}>")
        list(APPEND _extra_args NO_INSTALL
            RUNTIME_DIRECTORY ${_output}
            LIBRARY_DIRECTORY ${_output}
            ARCHIVE_DIRECTORY ${_output}
        )
    endif()

    helloutau_add_plugin(${_target}
        CATEGORY ${FUNC_CATEGORY}
        ${_extra_args}
        ${FUNC_UNPARSED_ARGUMENTS}
    )
    target_link_libraries(${_target} PRIVATE stdcorelib::plugin)
    target_include_directories(${_target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:${HELLOUTAU_INSTALL_INCLUDE_DIR}/helloutau/plugins>
    )
    stdc_add_plugin_metadata(TARGET ${_target} IID ${HELLOUTAU_PLUGIN_IID})

    set(_dependencies)
    foreach(_dependency IN LISTS FUNC_DEPENDENCIES)
        list(APPEND _dependencies "{\"id\": \"${_dependency}\", \"version\": \"${HELLOUTAU_VERSION}\", \"type\": \"required\"}")
    endforeach()
    list(JOIN _dependencies ", " _dependencies)

    # The base name includes the configuration postfix, if any.
    set(_metadata "{\n"
        "    \"name\": \"$<TARGET_FILE_BASE_NAME:${_target}>\",\n"
        "    \"id\": \"${FUNC_ID}\",\n"
        "    \"displayName\": \"${FUNC_DISPLAY_NAME}\",\n"
        "    \"version\": \"${HELLOUTAU_VERSION}\",\n"
        "    \"dependencies\": [${_dependencies}]\n"
        "}\n"
    )
    string(JOIN "" _metadata ${_metadata})
    set(_metadata_file ${_plugins_dir}/${FUNC_CATEGORY}/plugin.json)
    file(GENERATE OUTPUT ${_metadata_file} CONTENT "${_metadata}")

    if(HELLOUTAU_INSTALL AND NOT FUNC_DIRECTORY)
        install(FILES ${_metadata_file}
            DESTINATION ${HELLOUTAU_INSTALL_PLUGINS_DIR}/${FUNC_CATEGORY}
        )
        file(GLOB _headers ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
        if(HELLOUTAU_DEVEL AND _headers)
            install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
                DESTINATION ${HELLOUTAU_INSTALL_INCLUDE_DIR}/helloutau/plugins/${FUNC_CATEGORY}
                FILES_MATCHING PATTERN "*.h"
                PATTERN "*_p.h" EXCLUDE
                PATTERN "Internal" EXCLUDE
            )
        endif()
    endif()
endfunction()

add_subdirectory(Core)
add_subdirectory(ClassicPluginHost)
add_subdirectory(FrequencyEditor)
add_subdirectory(Interchange)
