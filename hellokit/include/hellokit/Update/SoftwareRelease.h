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

    /// A published release of an application, in the format of the release interface of GitHub,
    /// which Gitee and other hosts share. See the automatic update in docs/Distribution.md.
    struct HELLOKIT_UPDATE_EXPORT SoftwareRelease {
        QString tag;

        /// The version that the tag names, see versionOfTag().
        QVersionNumber version;

        QString title;

        /// The release notes in Markdown.
        QString notes;

        /// The web page of the release.
        QUrl page;

        /// The attached files. Some hosts, such as Gitee, also list the source archives that they
        /// generate.
        QList<SoftwareReleaseAsset> assets;

        /// Reads a release from \a object. The page is the \c html_url of \a object, or else
        /// \a pageTemplate with <tt>${tag}</tt> replaced by the tag, for a host without
        /// \c html_url such as Gitee. An asset without an HTTPS URL is omitted.
        ///
        /// \return the release, or \c std::nullopt if \a object is not a published release whose
        ///         tag names a version, such as a draft or a prerelease
        static std::optional<SoftwareRelease> fromJson(const QJsonObject &object,
                                                       const QString &pageTemplate = {});

        /// Returns \a pageTemplate with each <tt>${tag}</tt> replaced by \a tag in percent
        /// encoding, see \c stdc::str::varexp(). Other names are replaced by an empty string.
        static QUrl pageOf(const QString &pageTemplate, const QString &tag);

        /// Returns the version that \a tag names, the letter \c v followed by the dot-separated
        /// segments, such as \c v0.1.6.0, or \c std::nullopt if \a tag is not of this form.
        static std::optional<QVersionNumber> versionOfTag(const QString &tag);

        /// Returns the asset named \a name, or \c std::nullopt if absent.
        std::optional<SoftwareReleaseAsset> asset(const QString &name) const;
    };

}

#endif // HELLOKIT_UPDATE_SOFTWARERELEASE_H
