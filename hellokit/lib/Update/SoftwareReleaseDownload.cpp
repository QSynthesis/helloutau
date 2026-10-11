#include "SoftwareReleaseDownload.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSaveFile>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include "Sha256Sums.h"
#include "UpdateNetwork_p.h"

namespace hello::kit {

    namespace {

        constexpr char sumsName[] = "SHA256SUMS";

        // The largest SHA256SUMS file that is read, in bytes
        constexpr qint64 maximumSumsSize = 1 << 20;

        bool isPlainFileName(const QString &name) {
            return !name.isEmpty() && name != QLatin1String(".") && name != QLatin1String("..") &&
                   !name.contains(QLatin1Char('/')) && !name.contains(QLatin1Char('\\')) &&
                   !name.contains(QLatin1Char(':'));
        }

    }

    SoftwareReleaseDownload::SoftwareReleaseDownload(QObject *parent)
        : QObject(parent), m_manager(UpdateNetwork::createManager(this)) {
    }

    SoftwareReleaseDownload::~SoftwareReleaseDownload() {
        abort();
    }

    bool SoftwareReleaseDownload::start(const SoftwareRelease &release, const QString &assetName,
                                        const QString &directory) {
        abort();
        m_filePath.clear();
        m_errorString.clear();
        m_digest.clear();
        if (!isPlainFileName(assetName)) {
            m_errorString = tr("The name %1 is not a file name.").arg(assetName);
            return false;
        }
        const auto asset = release.asset(assetName);
        const auto sums = release.asset(QLatin1String(sumsName));
        if (!asset || !sums) {
            m_errorString = tr("The release %1 lacks the file %2 or %3.")
                                .arg(release.tag, assetName, QLatin1String(sumsName));
            return false;
        }
        // SoftwareRelease omits the assets without an HTTPS URL, but a release may be built by
        // other means.
        if (!UpdateNetwork::isHttps(asset->url) || !UpdateNetwork::isHttps(sums->url)) {
            m_errorString =
                tr("The release %1 has a file without an HTTPS address.").arg(release.tag);
            return false;
        }
        m_asset = *asset;
        m_destination = QDir(directory).absoluteFilePath(assetName);
        m_reply = m_manager->get(UpdateNetwork::requestOf(sums->url));
        connect(m_reply, &QNetworkReply::finished, this, &SoftwareReleaseDownload::readSums);
        return true;
    }

    void SoftwareReleaseDownload::abort() {
        releaseReply();
        if (m_file) {
            m_file->cancelWriting();
            m_file.reset();
        }
    }

    bool SoftwareReleaseDownload::isRunning() const {
        return m_reply;
    }

    QString SoftwareReleaseDownload::filePath() const {
        return m_filePath;
    }

    QString SoftwareReleaseDownload::errorString() const {
        return m_errorString;
    }

    void SoftwareReleaseDownload::readSums() {
        const auto reply = m_reply.data();
        if (reply->error() != QNetworkReply::NoError) {
            fail(reply->errorString());
            return;
        }
        if (reply->bytesAvailable() > maximumSumsSize) {
            fail(tr("The file %1 is too large.").arg(QLatin1String(sumsName)));
            return;
        }
        const auto digest =
            Sha256Sums::digest(reply->readAll(), QFileInfo(m_destination).fileName());
        releaseReply();
        if (!digest) {
            fail(tr("The file %1 lists no digest of %2.")
                     .arg(QLatin1String(sumsName), QFileInfo(m_destination).fileName()));
            return;
        }
        m_digest = *digest;

        // A download of an earlier start, or of an earlier session, is used again.
        if (QFileInfo::exists(m_destination) && Sha256Sums::fileDigest(m_destination) == m_digest) {
            m_filePath = m_destination;
            Q_EMIT finished();
            return;
        }
        if (!QDir().mkpath(QFileInfo(m_destination).absolutePath())) {
            fail(tr("The folder of %1 cannot be created.").arg(m_destination));
            return;
        }
        // QSaveFile writes to a temporary file and renames it on commit().
        m_file = std::make_unique<QSaveFile>(m_destination);
        if (!m_file->open(QIODevice::WriteOnly)) {
            fail(tr("The file %1 cannot be written: %2").arg(m_destination, m_file->errorString()));
            return;
        }
        m_reply = m_manager->get(UpdateNetwork::requestOf(m_asset.url));
        connect(m_reply, &QNetworkReply::readyRead, this, &SoftwareReleaseDownload::writeAsset);
        connect(m_reply, &QNetworkReply::downloadProgress, this,
                [this](qint64 received, qint64 total) {
                    if (received > maximumSize || total > maximumSize) {
                        fail(tr("The file %1 is too large.").arg(m_asset.name));
                        return;
                    }
                    Q_EMIT progressChanged(received, total);
                });
        connect(m_reply, &QNetworkReply::finished, this, &SoftwareReleaseDownload::readAsset);
    }

    void SoftwareReleaseDownload::writeAsset() {
        const auto data = m_reply->readAll();
        if (m_file->write(data) != data.size()) {
            fail(tr("The file %1 cannot be written: %2").arg(m_destination, m_file->errorString()));
        }
    }

    void SoftwareReleaseDownload::readAsset() {
        if (m_reply->error() != QNetworkReply::NoError) {
            fail(m_reply->errorString());
            return;
        }
        writeAsset();
        if (!m_file) {
            return;
        }
        releaseReply();
        const bool committed = m_file->commit();
        const auto error = m_file->errorString();
        m_file.reset();
        if (!committed) {
            fail(tr("The file %1 cannot be written: %2").arg(m_destination, error));
            return;
        }
        if (Sha256Sums::fileDigest(m_destination) != m_digest) {
            QFile::remove(m_destination);
            fail(tr("The downloaded file %1 does not match its SHA-256 digest.").arg(m_asset.name));
            return;
        }
        m_filePath = m_destination;
        Q_EMIT finished();
    }

    void SoftwareReleaseDownload::fail(const QString &message) {
        abort();
        m_errorString = message;
        Q_EMIT finished();
    }

    void SoftwareReleaseDownload::releaseReply() {
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

}
