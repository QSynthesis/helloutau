# Shared by the packaging scripts of macOS and Linux, which source it after setting build, the
# absolute path of the build directory. Sets version, triplet and base, the name of the assets
# without their suffixes, as package.ps1 of Windows names them:
#
#     helloutau-<version>-<triplet>                    a build of the channel release
#     helloutau-<version>-<channel>.<commit>-<triplet>  a build of the channels ci and dev
#
# The commit is left out if the build records none, as for sources outside a git repository,
# as SoftwareBuildChannel::buildName() leaves it out.

# Prints the value of the entry $1 of the cache of the build, or nothing if absent
cache_value() {
    sed -n "s/^$1:[A-Z]*=//p" "$build/CMakeCache.txt" | head -n 1
}

if [ ! -f "$build/CMakeCache.txt" ]; then
    echo "$build is no build directory of CMake." >&2
    exit 1
fi

version="$(cache_value CMAKE_PROJECT_VERSION)"
triplet="$(cache_value HELLO_TRIPLET)"
if [ -z "$version" ] || [ -z "$triplet" ]; then
    echo "The version or the triplet is missing from the cache of $build." >&2
    exit 1
fi

# The deployment is an install rule, which a build without HELLO_DEPLOY or a Debug build lacks
case "$(cache_value HELLO_DEPLOY)" in
    ON | TRUE | 1 | YES) ;;
    *)
        echo "HELLO_DEPLOY is off in $build. Configure the build with -DHELLO_DEPLOY=ON." >&2
        exit 1
        ;;
esac
if [ "$(cache_value CMAKE_BUILD_TYPE)" = Debug ]; then
    echo "$build is a Debug build, which is not deployed. Package a Release build." >&2
    exit 1
fi

channel="$(cache_value HELLO_BUILD_CHANNEL)"
case "$channel" in
    release) base="helloutau-$version-$triplet" ;;
    ci | dev)
        # hellokit/BuildInfo.h is in the include directory of the build, in Contents/Resources
        # of the bundle on macOS
        build_info="$(find "$build/out" -path '*/include/hellokit/BuildInfo.h' | head -n 1)"
        commit=""
        if [ -n "$build_info" ]; then
            commit="$(sed -n 's/^#define HELLOKIT_GIT_LAST_COMMIT_HASH "\([0-9a-f]\{7\}\).*/\1/p' \
                "$build_info")"
        fi
        if [ -n "$commit" ]; then
            base="helloutau-$version-$channel.$commit-$triplet"
        else
            base="helloutau-$version-$channel-$triplet"
        fi
        ;;
    *)
        echo "HELLO_BUILD_CHANNEL is '$channel' in $build, not release, ci or dev." >&2
        exit 1
        ;;
esac
