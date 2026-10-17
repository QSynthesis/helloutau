#include "SoftwareBuildChannel.h"

#include <string_view>

#include <hellokit/BuildInfo.h>
#include <hellokit/Config.h>

#include "helloutau/Config.h"

namespace hello::daw {

    namespace {

        constexpr std::string_view channel = HELLOKIT_BUILD_CHANNEL;
        constexpr bool release = channel == "release";

        // The channel as spelled in the name of the build
        constexpr const char *channelName = channel == "ci" ? "CI" : HELLOKIT_BUILD_CHANNEL;

        constexpr int abbreviatedCommitLength = 7;

    }

    bool SoftwareBuildChannel::isRelease() {
        return release;
    }

    QString SoftwareBuildChannel::buildName() {
        if (release) {
            return QStringLiteral(HELLOKIT_VERSION);
        }
        return QStringLiteral("%1 %2").arg(
            QLatin1String(channelName),
            QStringLiteral(HELLOKIT_GIT_LAST_COMMIT_HASH).left(abbreviatedCommitLength));
    }

    QString SoftwareBuildChannel::applicationTitle() {
        return QStringLiteral(HELLOUTAU_APPLICATION_NAME " %1").arg(buildName());
    }

}
