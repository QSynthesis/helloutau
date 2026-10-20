#!/bin/bash
# Packages a release build of HelloUtau for Linux into the release assets of
# docs/Distribution.md, in the output directory:
#
#     helloutau-<version>-<triplet>.tar.xz          the portable package, a folder HelloUtau
#     helloutau-<version>-<triplet>-symbols.tar.xz  the debug information of the binaries, a
#                                                   file <binary>.debug beside the path of each
#                                                   binary in the layout of the package, if the
#                                                   build records debug information
#     SHA256SUMS                                    the digests of the files above, as sha256sum
#                                                   writes them
#
# A build of another channel than release follows the version with -<channel>.<commit>, see
# common.sh. The build must be complete: this script does not build. Steps: cmake --install,
# which deploys Qt and the other libraries (HELLO_DEPLOY, see helloutau/CMakeLists.txt), remove
# the development files, move the debug information out of the binaries, and archive.
#
#     dist/installer/linux/package.sh [<build dir> [<output dir>]]
#
# The build directory defaults to build/Release of the repository, the output directory to
# package in the build directory.
set -euo pipefail

repository="$(cd "$(dirname "$0")/../../.." && pwd)"
build="$(cd "${1:-$repository/build/Release}" && pwd)"
output="${2:-$build/package}"
mkdir -p "$output"
output="$(cd "$output" && pwd)"

source "$repository/dist/installer/common.sh"
echo "Packaging $base from $build"

staging="$output/HelloUtau"
symbols="$output/symbols"
rm -rf "$staging" "$symbols"
rm -f "$output/$base.tar.xz" "$output/$base-symbols.tar.xz" "$output/SHA256SUMS"

echo "== Install"
cmake --install "$build" --prefix "$staging"

echo "== Remove the development files"
rm -rf "$staging/include" "$staging/lib/cmake"
find "$staging" -name '*.a' -type f -delete

# The scripts that register the folder with the desktop of the user and remove the registration,
# in the root of the package rather than in the installation, because an installation into /usr
# needs neither
echo "== Add the scripts of the desktop"
install -m 755 "$repository/dist/installer/linux/register-desktop.sh" \
    "$repository/dist/installer/linux/unregister-desktop.sh" "$staging"

# The debug information of each binary moves into <binary>.debug at the path of the binary
# relative to the output directory, and the binary keeps its symbol table and a .gnu_debuglink to
# that file. gdb finds the file beside the binary once the symbols are extracted over the package.
# A binary without debug information, such as a library of Qt, is left as it is.
echo "== Collect the debug symbols"
total=0
count=0
while IFS= read -r -d '' file; do
    if ! file -b "$file" | grep -q '^ELF'; then
        continue
    fi
    total=$((total + 1))
    if ! readelf -S "$file" | grep -q '\.debug_info'; then
        continue
    fi
    relative="${file#"$output"/}"
    debug="$symbols/$relative.debug"
    mkdir -p "$(dirname "$debug")"
    objcopy --only-keep-debug "$file" "$debug"
    objcopy --strip-debug --add-gnu-debuglink="$debug" "$file"
    count=$((count + 1))
done < <(find "$staging" -type f -print0)
echo "  $count of $total binaries have symbols"

# tar keeps the modes and the links of the libraries. The entries name no user or group of the
# build machine, and xz compresses on every processor.
archive() {
    XZ_OPT=-T0 tar --owner=0 --group=0 --numeric-owner -cJf "$@"
}

echo "== Create $base.tar.xz"
(cd "$output" && archive "$base.tar.xz" HelloUtau)
assets=("$base.tar.xz")

if [ "$count" -gt 0 ]; then
    echo "== Create $base-symbols.tar.xz"
    (cd "$symbols" && archive "$output/$base-symbols.tar.xz" .)
    assets+=("$base-symbols.tar.xz")
fi

(cd "$output" && sha256sum "${assets[@]}" > SHA256SUMS)
cat "$output/SHA256SUMS"
echo "== Packaged into $output"
