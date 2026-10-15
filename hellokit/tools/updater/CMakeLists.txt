project(hello-updater
    VERSION ${HELLO_VERSION}
    DESCRIPTION "HelloUtau Updater"
    LANGUAGES CXX
)

# The program that replaces the installation directory of a portable application with the new
# version that the application has extracted beside it. See main.cpp and docs/Distribution.md.
#
# The application runs a copy outside the installation directory, where no library of the
# application is available. The program therefore links neither Qt nor any shared library of
# this repository, and links the C++ runtime statically. Its only library of this repository is
# the static HelloKitWinMain on Windows, which requires no particular runtime. It is a GUI program
# on Windows, so that no console window appears.
hellokit_add_executable(${PROJECT_NAME}
    SOURCES main.cpp
    WINDOWS
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

# hellokit/Config.h, the version of which --version shows. The header is generated, and the
# program links no library of the repository.
target_include_directories(${PROJECT_NAME} PRIVATE ${HELLOKIT_BUILD_INCLUDE_DIR})

# The static runtime of the MSVC ABI, which clang targeting x86_64-pc-windows-msvc also uses.
# Compilers of other ABIs ignore the property.
set_target_properties(${PROJECT_NAME} PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

if(WIN32)
    # The entry point of the GUI program, which passes the arguments to main in UTF-8
    target_link_libraries(${PROJECT_NAME} PRIVATE HelloKitWinMain)
    if(MINGW)
        target_link_options(${PROJECT_NAME} PRIVATE -static)
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # glibc is not linked statically, because its name resolution loads shared modules.
    target_link_options(${PROJECT_NAME} PRIVATE -static-libgcc -static-libstdc++)
endif()
