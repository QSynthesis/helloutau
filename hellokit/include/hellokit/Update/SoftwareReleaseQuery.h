#ifndef HELLOKIT_UPDATE_SOFTWARERELEASEQUERY_H
#define HELLOKIT_UPDATE_SOFTWARERELEASEQUERY_H

#include <optional>

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QUrl>

#include <hellokit/Update/SoftwareRelease.h>

class QNetworkAccessManager;
class QNetworkReply;

namespace hello::kit {

    /// A request for the latest release of a repository through the public release interface of
    /// its host, such as <tt>https://api.github.com/repos/owner/repo/releases/latest</tt> for
    /// the repository \c owner/repo on GitHub, which requires no token for a public repository.
    /// The address is used as given, without expansion. Only HTTPS is accepted, and a redirect
    /// to HTTP fails the request. The proxy of the system is used.
    class HELLOKIT_UPDATE_EXPORT SoftwareReleaseQuery : public QObject {
        Q_OBJECT
    public:
        explicit SoftwareReleaseQuery(QObject *parent = nullptr);
        ~SoftwareReleaseQuery();

        /// Requests the release at \a url, and emits finished() once the reply arrives. The page
        /// of the release follows \a pageTemplate if the reply has none, see
        /// SoftwareRelease::fromJson(). A running request is aborted without finished().
        ///
        /// \return whether the request was sent. If \a url is not an HTTPS URL, nothing is sent,
        ///         finished() is not emitted, and errorString() returns the reason.
        bool start(const QUrl &url, const QString &pageTemplate = {});

        /// Aborts the running request without finished().
        void abort();

        /// Returns whether a request is running.
        bool isRunning() const;

        /// Returns the release of the last finished request, or \c std::nullopt if it failed,
        /// with the reason in errorString().
        const std::optional<SoftwareRelease> &release() const;

        /// Returns the reason why the last finished request failed, or an empty string if it
        /// succeeded.
        QString errorString() const;

    Q_SIGNALS:
        void finished();

    private:
        void readReply();

        QNetworkAccessManager *m_manager;
        QPointer<QNetworkReply> m_reply;
        QString m_pageTemplate;
        std::optional<SoftwareRelease> m_release;
        QString m_errorString;
    };

}

#endif // HELLOKIT_UPDATE_SOFTWARERELEASEQUERY_H
