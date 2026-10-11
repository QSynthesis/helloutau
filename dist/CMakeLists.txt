# The values of the build that the packaging scripts read, such as
# dist/installer/windows/package.ps1, one NAME=value on each line. Rewritten only if a value
# changes.
file(CONFIGURE
    OUTPUT "${CMAKE_BINARY_DIR}/package.txt"
    CONTENT "HELLO_VERSION=${HELLO_VERSION}\nHELLO_TRIPLET=${HELLO_TRIPLET}\n"
)
