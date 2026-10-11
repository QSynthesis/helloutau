#include "SoftwareReleaseQuery.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include "UpdateNetwork_p.h"

namespace hello::kit {

    SoftwareReleaseQuery::SoftwareReleaseQuery(QObject *parent)
        : QObject(parent), m_manager(UpdateNetwork::createManager(this)) {
    }

    SoftwareReleaseQuery::~SoftwareReleaseQuery() {
        abort();
    }

    bool SoftwareReleaseQuery::start(const QUrl &url, const QString &pageTemplate) {
        abort();
        m_pageTemplate = pageTemplate;
        m_release.reset();
        m_errorString.clear();
        if (!UpdateNetwork::isHttps(url)) {
            m_errorString = tr("The address %1 is not an HTTPS address.").arg(url.toString());
            return false;
        }

        m_reply = m_manager->get(UpdateNetwork::requestOf(url));
        connect(m_reply, &QNetworkReply::finished, this, &SoftwareReleaseQuery::readReply);
        return true;
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
                m_release = SoftwareRelease::fromJson(document.object(), m_pageTemplate);
                if (!m_release) {
                    m_errorString = tr("The latest release does not name a version.");
                }
            }
        }
        Q_EMIT finished();
    }

}
