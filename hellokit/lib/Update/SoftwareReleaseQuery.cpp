#include "SoftwareReleaseQuery.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkProxyFactory>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace hello::kit {

    namespace {

        // A request that receives no data for this long fails.
        constexpr int transferTimeout = 30000;

        // The proxy of the system for the requests of one manager. The global
        // QNetworkProxyFactory::setUseSystemConfiguration() would apply to every manager of the
        // application.
        class SystemProxyFactory : public QNetworkProxyFactory {
        public:
            QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery &query) override {
                return systemProxyForQuery(query);
            }
        };

    }

    SoftwareReleaseQuery::SoftwareReleaseQuery(QObject *parent)
        : QObject(parent), m_manager(new QNetworkAccessManager(this)) {
        m_manager->setProxyFactory(new SystemProxyFactory());
        m_manager->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
        m_manager->setTransferTimeout(transferTimeout);
    }

    SoftwareReleaseQuery::~SoftwareReleaseQuery() {
        abort();
    }

    QUrl SoftwareReleaseQuery::latestReleaseUrl(SoftwareRelease::Host host,
                                                const QString &repository) {
        if (host == SoftwareRelease::Gitee) {
            return QUrl(QStringLiteral("https://gitee.com/api/v5/repos/") + repository +
                        QStringLiteral("/releases/latest"));
        }
        return QUrl(QStringLiteral("https://api.github.com/repos/") + repository +
                    QStringLiteral("/releases/latest"));
    }

    void SoftwareReleaseQuery::start(SoftwareRelease::Host host, const QString &repository) {
        abort();
        m_host = host;
        m_repository = repository;
        m_release.reset();
        m_errorString.clear();

        QNetworkRequest request(latestReleaseUrl(host, repository));
        // GitHub rejects a request without a user agent.
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          QCoreApplication::applicationName() + QLatin1Char('/') +
                              QCoreApplication::applicationVersion());
        if (host == SoftwareRelease::GitHub) {
            request.setRawHeader("Accept", "application/vnd.github+json");
        }
        m_reply = m_manager->get(request);
        connect(m_reply, &QNetworkReply::finished, this, &SoftwareReleaseQuery::readReply);
    }

    void SoftwareReleaseQuery::abort() {
        if (!m_reply) {
            return;
        }
        // Disconnected first, because abort() emits finished() of the reply.
        const auto reply = m_reply.data();
        m_reply = nullptr;
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }

    bool SoftwareReleaseQuery::isRunning() const {
        return m_reply;
    }

    const std::optional<SoftwareRelease> &SoftwareReleaseQuery::release() const {
        return m_release;
    }

    QString SoftwareReleaseQuery::errorString() const {
        return m_errorString;
    }

    void SoftwareReleaseQuery::readReply() {
        const auto reply = m_reply.data();
        m_reply = nullptr;
        reply->deleteLater();

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 404) {
            m_errorString = tr("No release has been published.");
        } else if (reply->error() != QNetworkReply::NoError) {
            m_errorString = reply->errorString();
        } else {
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(reply->readAll(), &error);
            if (error.error != QJsonParseError::NoError || !document.isObject()) {
                m_errorString = tr("The server returned an invalid response.");
            } else {
                m_release = SoftwareRelease::fromJson(document.object(), m_host, m_repository);
                if (!m_release) {
                    m_errorString = tr("The latest release does not name a version.");
                }
            }
        }
        Q_EMIT finished();
    }

}
