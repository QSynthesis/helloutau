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

    /// A request for the latest release of a repository on GitHub or Gitee through the public
    /// interface of the host, which requires no token for a public repository. Only HTTPS is
    /// accepted, and a redirect to HTTP fails the request. The proxy of the system is used.
    class HELLOKIT_UPDATE_EXPORT SoftwareReleaseQuery : public QObject {
        Q_OBJECT
    public:
        explicit SoftwareReleaseQuery(QObject *parent = nullptr);
        ~SoftwareReleaseQuery();

        /// Returns the address of the latest release of \a repository, written as
        /// <tt>owner/name</tt>, in the interface of \a host.
        static QUrl latestReleaseUrl(SoftwareRelease::Host host, const QString &repository);

        /// Requests the latest release of \a repository on \a host, and emits finished() once
        /// the reply arrives. A running request is aborted without finished().
        void start(SoftwareRelease::Host host, const QString &repository);

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
        SoftwareRelease::Host m_host = SoftwareRelease::GitHub;
        QString m_repository;
        std::optional<SoftwareRelease> m_release;
        QString m_errorString;
    };

}

#endif // HELLOKIT_UPDATE_SOFTWARERELEASEQUERY_H
