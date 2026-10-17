# The flags of AddressSanitizer for the code of this repository, included if
# HELLO_ENABLE_ADDRESS_SANITIZER is on. See the variant asan of the workflow in
# docs/Distribution.md.
#
# The spelling of the flags follows the front end of the compiler, not the platform: cl takes
# /fsanitize=address, and clang with the GNU front end takes -fsanitize=address on every system.
# MSVC supports neither incremental linking nor /RTC with AddressSanitizer.
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    add_compile_options(/fsanitize=address)
    add_link_options(/INCREMENTAL:NO)
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
else()
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

# The libraries from vcpkg are built without AddressSanitizer. A container of the standard
# library of MSVC that is annotated on one side of such a library and not on the other is
# reported as overflowing, and a static library with other annotations fails to link, so all the
# annotations are turned off. Overflows into the reserved capacity of std::vector and
# std::string, and accesses to an empty std::optional, are therefore not detected.
#
# _DISABLE_STL_ANNOTATION rather than one macro for each container: the standard library of MSVC
# 14.51 annotates std::optional as well, which 14.44 does not. With the macros of std::vector and
# std::string alone, the poison of an std::optional stayed on the stack where code without
# AddressSanitizer, such as stdutau, had used it, and was later reported as use-after-poison.
if(WIN32)
    add_compile_definitions(_DISABLE_STL_ANNOTATION)
endif()
