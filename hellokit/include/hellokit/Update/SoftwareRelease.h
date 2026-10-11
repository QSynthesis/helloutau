#ifndef HELLOKIT_UPDATE_SOFTWARERELEASE_H
#define HELLOKIT_UPDATE_SOFTWARERELEASE_H

#include <optional>

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtCore/QVersionNumber>

#include <hellokit/Update/HelloKitUpdateGlobal.h>

class QJsonObject;

namespace hello::kit {

    /// A file attached to a release.
    struct SoftwareReleaseAsset {
        QString name;

        /// The download URL, always an HTTPS URL.
        QUrl url;
    };

    /// A published release of an application on GitHub or Gitee, as the release interface of
    /// the host returns it. See the automatic update in docs/Distribution.md.
    struct HELLOKIT_UPDATE_EXPORT SoftwareRelease {
        /// The hosts of the releases, which differ in the format of a release.
        enum Host {
            GitHub,
            Gitee,
        };

        QString tag;

        /// The version that the tag names, see versionOfTag().
        QVersionNumber version;

        QString title;

        /// The release notes in Markdown.
        QString notes;

        /// The web page of the release.
        QUrl page;

        /// The attached files. A Gitee release also lists the source archives that Gitee
        /// generates.
        QList<SoftwareReleaseAsset> assets;

        /// Reads a release of \a repository, written as <tt>owner/name</tt>, from \a object in
        /// the format of \a host. An asset without an HTTPS URL is omitted.
        ///
        /// \return the release, or \c std::nullopt if \a object is not a published release whose
        ///         tag names a version, such as a draft or a prerelease
        static std::optional<SoftwareRelease> fromJson(const QJsonObject &object, Host host,
                                                       const QString &repository);

        /// Returns the version that \a tag names, the letter \c v followed by the dot-separated
        /// segments, such as \c v0.1.6.0, or \c std::nullopt if \a tag is not of this form.
        static std::optional<QVersionNumber> versionOfTag(const QString &tag);

        /// Returns the asset named \a name, or \c std::nullopt if absent.
        std::optional<SoftwareReleaseAsset> asset(const QString &name) const;
    };

}

#endif // HELLOKIT_UPDATE_SOFTWARERELEASE_H
