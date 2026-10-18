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

    }

    bool SoftwareBuildChannel::isRelease() {
        return release;
    }

    QString SoftwareBuildChannel::buildName() {
        if (release) {
            return QStringLiteral(HELLOKIT_VERSION);
        }
        return QStringLiteral(HELLOKIT_VERSION "-%1.%2")
            .arg(QLatin1String(HELLOKIT_BUILD_CHANNEL),
                 QStringLiteral(HELLOKIT_GIT_LAST_COMMIT_HASH).left(abbreviatedCommitLength));
    }

    QString SoftwareBuildChannel::applicationTitle() {
        return QStringLiteral(HELLOUTAU_APPLICATION_NAME " %1").arg(buildName());
    }

}
