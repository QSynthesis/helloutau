# stdutau is developed alongside this repository rather than consumed as a release, so it is built
# from the checkout next to this one. Clone it as a sibling of helloutau.
set(_stdutau_dir ${HELLO_SOURCE_DIR}/../stdutau)

if(NOT EXISTS ${_stdutau_dir}/CMakeLists.txt)
    message(FATAL_ERROR
        "stdutau not found at ${_stdutau_dir}. Clone https://github.com/diffscope/stdutau next to "
        "this repository, or point HELLO_STDUTAU_DIR at your checkout.")
endif()

add_subdirectory(${_stdutau_dir} ${CMAKE_CURRENT_BINARY_DIR}/stdutau)
