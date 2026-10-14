add_subdirectory(fswatcher)
add_subdirectory(updater)

# Classic plugins run only on Windows. See docs/PluginPlay.md.
if(WIN32)
    add_subdirectory(plugin-play)
endif()
