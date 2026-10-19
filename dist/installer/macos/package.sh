#!/bin/bash
# Packages a release build of HelloUtau for macOS into the release assets of
# docs/Distribution.md, in the output directory:
#
#     helloutau-<version>-<triplet>.dmg          the disk image, with HelloUtau.app and a link
#                                                to /Applications, which shows the license
#                                                before it opens
#     helloutau-<version>-<triplet>-symbols.zip  the dSYMs of the binaries, in the layout of the
#                                                bundle, if the build records debug information
#     SHA256SUMS                                 the digests of the files above, as sha256sum
#                                                writes them
#
# A build of another channel than release follows the version with -<channel>.<commit>, see
# common.sh. The build must be complete: this script does not build. Steps: cmake --install,
# which deploys Qt and the other libraries into the bundle (HELLO_DEPLOY, see
# helloutau/CMakeLists.txt), remove the development files, extract the debug information, sign
# the bundle ad hoc, and create the disk image.
#
#     dist/installer/macos/package.sh [<build dir> [<output dir>]]
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

# The root of the disk image, which the installation fills with HelloUtau.app
image="$output/image"
symbols="$output/symbols"
rm -rf "$image" "$symbols"
rm -f "$output/$base.dmg" "$output/$base-symbols.zip" "$output/SHA256SUMS"

echo "== Install"
cmake --install "$build" --prefix "$image"
app="$image/HelloUtau.app"
if [ ! -d "$app" ]; then
    echo "The installation holds no HelloUtau.app. Configure the build on macOS." >&2
    exit 1
fi

echo "== Remove the development files"
rm -rf "$app/Contents/Resources/include" "$app/Contents/Resources/lib"
find "$app" -name '*.a' -type f -delete

# Lists the Mach-O files of the bundle outside the frameworks, whose binaries are signed with
# their framework, NUL-separated
macho_files() {
    find "$app/Contents" -type f -print0 | while IFS= read -r -d '' file; do
        case "$file" in
            *.framework/*) ;;
            *) if file -b "$file" | grep -q '^Mach-O'; then printf '%s\0' "$file"; fi ;;
        esac
    done
}

# The debug information of each binary, in a dSYM at the path of the binary relative to the image,
# for the analysis of crash reports. dsymutil collects it from the object files of the build,
# which the debug map of the binary names. A binary without debug information, such as a library
# of Qt or a library of vcpkg restored from the binary cache without its objects, gets no dSYM.
echo "== Collect the debug symbols"
total=0
count=0
while IFS= read -r -d '' file; do
    total=$((total + 1))
    relative="${file#"$image"/}"
    dsym="$symbols/$relative.dSYM"
    mkdir -p "$(dirname "$dsym")"
    dsymutil "$file" -o "$dsym" > /dev/null 2>&1 || true
    dwarf="$dsym/Contents/Resources/DWARF/$(basename "$file")"
    if [ -f "$dwarf" ] && otool -l "$dwarf" | grep -q 'sectname __debug_info'; then
        count=$((count + 1))
    else
        rm -rf "$dsym"
    fi
done < <(macho_files)
echo "  $count of $total binaries have symbols"

# Apple Silicon runs signed code only. The deployment changed the run paths of the binaries,
# which invalidates their signatures, so the bundle is signed again ad hoc from the inside out:
# the binaries outside the frameworks, the frameworks, then the bundle itself.
echo "== Sign"
while IFS= read -r -d '' file; do
    codesign --force --sign - "$file"
done < <(macho_files)
for framework in "$app"/Contents/Frameworks/*.framework; do
    if [ -d "$framework" ]; then
        codesign --force --sign - "$framework"
    fi
done
codesign --force --sign - "$app"
codesign --verify --deep --strict --verbose=2 "$app"

echo "== Create $base.dmg"
ln -s /Applications "$image/Applications"
hdiutil create -volname HelloUtau -srcfolder "$image" -ov -format UDZO "$output/$base.dmg"

# The license, which the Finder shows before it mounts the image, and which the user accepts
# before the image opens. hdiutil reports udifrez as deprecated, and it is the only way that
# hdiutil provides.
echo "== Add the license agreement"
python3 "$repository/dist/installer/macos/sla.py" "$app/Contents/Resources/LICENSE" \
    "$output/sla.plist"
hdiutil udifrez -xml "$output/sla.plist" '' -quiet "$output/$base.dmg"
assets=("$base.dmg")

if [ "$count" -gt 0 ]; then
    echo "== Create $base-symbols.zip"
    (cd "$symbols" && zip -qry "$output/$base-symbols.zip" .)
    assets+=("$base-symbols.zip")
fi

(cd "$output" && shasum -a 256 "${assets[@]}" > SHA256SUMS)
cat "$output/SHA256SUMS"
echo "== Packaged into $output"
