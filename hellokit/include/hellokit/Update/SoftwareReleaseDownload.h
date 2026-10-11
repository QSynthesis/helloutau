#ifndef HELLOKIT_UPDATE_SOFTWARERELEASEDOWNLOAD_H
#define HELLOKIT_UPDATE_SOFTWARERELEASEDOWNLOAD_H

#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>

#include <hellokit/Update/SoftwareRelease.h>

class QNetworkAccessManager;
class QNetworkReply;
class QSaveFile;

namespace hello::kit {

    /// The download of an asset of a release, verified against the \c SHA256SUMS asset of the
    /// same release. The \c SHA256SUMS file is downloaded first. The asset is written to a
    /// temporary file beside its destination and renamed only if the download is complete, and
    /// it is removed if its digest does not match. An existing file with the matching digest is
    /// used without downloading it again. The network access is that of SoftwareReleaseQuery.
    class HELLOKIT_UPDATE_EXPORT SoftwareReleaseDownload : public QObject {
        Q_OBJECT
    public:
        /// The largest asset that is downloaded, in bytes
        static constexpr qint64 maximumSize = qint64(1) << 30;

        explicit SoftwareReleaseDownload(QObject *parent = nullptr);
        ~SoftwareReleaseDownload();

        /// Downloads the asset named \a assetName of \a release into \a directory, which is
        /// created if absent, and emits finished() once the asset is verified or the download
        /// fails. A running download is aborted without finished().
        ///
        /// \return whether the download started. If \a release lacks the asset or
        ///         \c SHA256SUMS, if either has no HTTPS URL, or if \a assetName is not a plain
        ///         file name, nothing is downloaded, finished() is not emitted, and errorString()
        ///         returns the reason.
        bool start(const SoftwareRelease &release, const QString &assetName,
                   const QString &directory);

        /// Aborts the running download without finished(), and removes its temporary file.
        void abort();

        /// Returns whether a download is running.
        bool isRunning() const;

        /// Returns the path of the verified asset after a successful download, or an empty
        /// string otherwise.
        QString filePath() const;

        /// Returns the reason why the last finished download failed, or an empty string if it
        /// succeeded.
        QString errorString() const;

    Q_SIGNALS:
        /// Reports the progress of the asset, with \a total as -1 if the size is unknown.
        void progressChanged(qint64 received, qint64 total);

        void finished();

    private:
        void readSums();
        void writeAsset();
        void readAsset();
        void fail(const QString &message);
        void releaseReply();

        QNetworkAccessManager *m_manager;
        QPointer<QNetworkReply> m_reply;
        SoftwareReleaseAsset m_asset;
        QString m_destination;
        QByteArray m_digest;
        std::unique_ptr<QSaveFile> m_file;
        QString m_filePath;
        QString m_errorString;
    };

}

#endif // HELLOKIT_UPDATE_SOFTWARERELEASEDOWNLOAD_H
