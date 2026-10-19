#include "SoftwareBuildChannel.h"

#include <string_view>

#include <hellokit/BuildInfo.h>
#include <hellokit/Config.h>

#include "helloutau/Config.h"

namespace hello::daw {

    namespace {

        constexpr std::string_view channel = HELLOKIT_BUILD_CHANNEL;
        constexpr bool release = channel == "release";

        constexpr int abbreviatedCommitLength = 7;

        // qmsetup records unknown instead of the hash if the sources are not in a git repository,
        // such as an extracted archive of the sources.
        constexpr std::string_view commit = HELLOKIT_GIT_LAST_COMMIT_HASH;

        constexpr bool hasCommit = [] {
            if (commit.size() < abbreviatedCommitLength) {
                return false;
            }
            for (const auto c : commit) {
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
                    return false;
                }
            }
            return true;
        }();

    }

    bool SoftwareBuildChannel::isRelease() {
        return release;
    }

    QString SoftwareBuildChannel::buildName() {
        if (release) {
            return QStringLiteral(HELLOKIT_VERSION);
        }
        const auto name =
            QStringLiteral(HELLOKIT_VERSION "-%1").arg(QLatin1String(HELLOKIT_BUILD_CHANNEL));
        if (!hasCommit) {
            return name;
        }
        return name + QLatin1Char('.') +
               QStringLiteral(HELLOKIT_GIT_LAST_COMMIT_HASH).left(abbreviatedCommitLength);
    }

    QString SoftwareBuildChannel::applicationTitle() {
        return QStringLiteral(HELLOUTAU_APPLICATION_NAME " %1").arg(buildName());
    }

}
