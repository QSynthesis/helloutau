#ifndef HELLOUTAU_EDITOR_SOFTWAREBUILDCHANNEL_H
#define HELLOUTAU_EDITOR_SOFTWAREBUILDCHANNEL_H

#include <QtCore/QString>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    /// The channel of the build, HELLOKIT_BUILD_CHANNEL of hellokit/Config.h: a release, a build
    /// of the workflow, or a local build. A build of another channel than a release is named by
    /// its commit rather than by its version, so that it is not taken for a release. See the
    /// build channels in docs/Distribution.md.
    class HELLOUTAU_EDITOR_EXPORT SoftwareBuildChannel {
    public:
        /// Returns whether the build is a release.
        static bool isRelease();

        /// Returns the name of the build: the version for a release, such as \c 0.1.8.0 , and
        /// otherwise the channel and the abbreviated commit, such as <tt>ci 1a2b3c4</tt> or
        /// <tt>dev 1a2b3c4</tt>. The channel is an identifier and is not translated.
        static QString buildName();

        /// Returns the application name followed by buildName(), such as
        /// <tt>HelloUtau 0.1.8.0</tt>, which ends the window titles.
        static QString applicationTitle();
    };

}

#endif // HELLOUTAU_EDITOR_SOFTWAREBUILDCHANNEL_H
