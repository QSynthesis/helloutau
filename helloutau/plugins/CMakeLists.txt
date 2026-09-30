# The native plugins that come with the application, one directory each under
# ${HELLOUTAU_BUILD_PLUGINS_DIR}, the directory layout of stdcorelib.plugin. See docs/Plugins.md.

# The application has loaded every library a plugin links before it loads the plugin. The applocal
# deployment of vcpkg would instead copy the libraries that its installed tree holds beside each
# plugin, among them versions of stdcorelib other than the one the application runs.
set(VCPKG_APPLOCAL_DEPS OFF)

# helloutau_add_native_plugin(<target>
#     ID <id> DISPLAY_NAME <name> CATEGORY <directory>
#     [arguments of helloutau_add_plugin()...]
# )
#
# Builds the library into the directory <directory> of the plugins, embeds the interface ID
# HELLOUTAU_PLUGIN_IID, and writes the plugin.json beside it that the directory layout reads:
# the name of the library without prefix and suffix, the ID, the name shown and the version.
function(helloutau_add_native_plugin _target)
    set(options)
    set(oneValueArgs ID DISPLAY_NAME CATEGORY)
    set(multiValueArgs)
    cmake_parse_arguments(FUNC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    helloutau_add_plugin(${_target}
        CATEGORY ${FUNC_CATEGORY}
        NO_EXPORT
        ${FUNC_UNPARSED_ARGUMENTS}
    )
    target_link_libraries(${_target} PRIVATE stdcorelib::plugin)
    stdc_add_plugin_metadata(TARGET ${_target} IID ${HELLOUTAU_PLUGIN_IID})

    # The base name includes the postfix of the configuration, as in coreplugind.
    set(_metadata "{\n"
        "    \"name\": \"$<TARGET_FILE_BASE_NAME:${_target}>\",\n"
        "    \"id\": \"${FUNC_ID}\",\n"
        "    \"displayName\": \"${FUNC_DISPLAY_NAME}\",\n"
        "    \"version\": \"${HELLOUTAU_VERSION}\"\n"
        "}\n"
    )
    string(JOIN "" _metadata ${_metadata})
    set(_metadata_file ${HELLOUTAU_BUILD_PLUGINS_DIR}/${FUNC_CATEGORY}/plugin.json)
    file(GENERATE OUTPUT ${_metadata_file} CONTENT "${_metadata}")

    if(HELLOUTAU_INSTALL)
        install(FILES ${_metadata_file}
            DESTINATION ${HELLOUTAU_INSTALL_PLUGINS_DIR}/${FUNC_CATEGORY}
        )
    endif()
endfunction()

add_subdirectory(Core)
