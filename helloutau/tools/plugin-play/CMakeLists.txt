project(hello-plugin-play
    VERSION ${HELLO_VERSION}
    DESCRIPTION "HelloUtau Plugin Play"
    LANGUAGES CXX
)

# The preview program for classic plugins, which a plugin starts under the name playinplugin.exe.
# See main.cpp and docs/PluginPlay.md.
#
# The program runs in the folder of a plugin, where no library of the application is available.
# It therefore links neither Qt nor any library of this repository, and links the C++ runtime
# statically, as hello-updater does. It is a GUI program, so that no console window appears.
# Classic plugins run only on Windows, and the program is built only there.
helloutau_add_executable(${PROJECT_NAME}
    SOURCES main.cpp
    WINDOWS
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

# hellokit/Config.h, the version of which --version shows. The header is generated into the same
# include directory of the build as helloutau/Config.h, and the program links no library of the
# repository.
target_include_directories(${PROJECT_NAME} PRIVATE ${HELLOUTAU_BUILD_INCLUDE_DIR})

# The static runtime of the MSVC ABI, which clang targeting x86_64-pc-windows-msvc also uses.
# Compilers of other ABIs ignore the property.
set_target_properties(${PROJECT_NAME} PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

# The entry point is wmain, for the arguments in UTF-16, also in a GUI program.
if(MINGW)
    target_compile_options(${PROJECT_NAME} PRIVATE -municode)
    target_link_options(${PROJECT_NAME} PRIVATE -municode -static)
else()
    target_link_options(${PROJECT_NAME} PRIVATE "LINKER:/ENTRY:wmainCRTStartup")
endif()
