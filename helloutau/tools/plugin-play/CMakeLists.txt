project(hello-plugin-play
    VERSION ${HELLO_VERSION}
    DESCRIPTION "HelloUtau Plugin Play"
    LANGUAGES CXX
)

# The preview program for classic plugins, which a plugin starts under the name playinplugin.exe.
# See main.cpp and docs/PluginPlay.md.
#
# The program runs in the folder of a plugin, where no library of the application is available.
# It therefore links neither Qt nor any shared library of this repository, and links the C++
# runtime statically, as hello-updater does. Its only library of this repository is the static
# HelloKitWinMain, which requires no particular runtime. It is a GUI program, so that no console
# window appears.
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

# The entry point of the GUI program, which passes the arguments to main in UTF-8
target_link_libraries(${PROJECT_NAME} PRIVATE HelloKitWinMain)

# The manifest of qmsetup, for the visual styles of the message boxes
qm_add_win_manifest(${PROJECT_NAME})
if(MINGW)
    target_link_options(${PROJECT_NAME} PRIVATE -static)
endif()
