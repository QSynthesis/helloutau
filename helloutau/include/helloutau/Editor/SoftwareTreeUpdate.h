#ifndef HELLOUTAU_EDITOR_SOFTWARETREEUPDATE_H
#define HELLOUTAU_EDITOR_SOFTWARETREEUPDATE_H

#include <atomic>
#include <functional>
#include <memory>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QThread;

namespace hello::kit {
    class ZipExtractor;
}

namespace hello::daw {

    /// The update that replaces the file tree of the installation as a whole, as the portable
    /// application and the bundle of macOS are updated: the extraction of the new version into
    /// <tt><installation>.new</tt> beside the installation directory, and the replacement of the
    /// installation directory by \c hello-updater after the application quits. The installation
    /// directory of a bundle is the bundle. See the update of the portable application and the
    /// automatic update of macOS in docs/Distribution.md.
    class HELLOUTAU_EDITOR_EXPORT SoftwareTreeUpdate : public QObject {
        Q_OBJECT
    public:
        /// Constructs the update of the installation in \a installDirectory, as
        /// applicationInstallDirectory() returns it for the running application.
        explicit SoftwareTreeUpdate(const QString &installDirectory, QObject *parent = nullptr);

        /// Cancels a running extraction and waits for it to end.
        ~SoftwareTreeUpdate();

        /// Returns the installation directory of the running application: the bundle on macOS,
        /// and the parent of the directory of the program on other systems.
        static QString applicationInstallDirectory();

        /// Returns whether the bundle \a bundle is on a read-only volume, as in a mounted disk
        /// image, or in the copy of App Translocation, from which macOS runs a downloaded
        /// application that the user has not moved. An update cannot replace such a bundle.
        /// Returns false on other systems.
        static bool isIsolated(const QString &bundle);

        /// Returns the file name of \c hello-updater, with \c .exe on Windows.
        static QString updaterFileName();

        /// Returns the path of \c hello-updater in the directory of the program.
        static QString defaultUpdater();

        QString installDirectory() const;

        /// The directory of the new version, <tt><installation>.new</tt>
        QString newDirectory() const;

        /// The directory to which hello-updater renames the installation directory,
        /// <tt><installation>.old</tt>
        QString oldDirectory() const;

        /// Returns whether a directory can be created beside the installation directory, which
        /// the extraction and the renames of hello-updater require.
        bool canReplace() const;

        /// Extracts \a archive into newDirectory() on another thread, and emits finished() once
        /// the extraction ends. An existing newDirectory() is removed first. The single top
        /// directory of the archive is removed from the paths, and the extraction fails unless
        /// the file \a program, relative to the installation directory, then exists. If the
        /// extraction fails, newDirectory() is removed. A running extraction is canceled first.
        void startExtraction(const QString &archive, const QString &program, int threadCount);

        /// Copies the single bundle at the root of the disk image \a diskImage into
        /// newDirectory() on another thread, as startExtraction() extracts an archive, and emits
        /// finished() once the copy ends. The image is mounted with \c hdiutil, which accepts its
        /// license agreement, and the bundle is copied with \c ditto, which keeps the symbolic
        /// links of the frameworks. The copy reports no progress. Only on macOS. On other
        /// systems the copy fails.
        void startImageCopy(const QString &diskImage, const QString &program);

        /// Cancels the running extraction or copy without finished(). newDirectory() is removed
        /// on the thread of the extraction once it stops.
        void cancel();

        /// Returns whether an extraction is running.
        bool isRunning() const;

        /// Returns the reason why the last finished extraction failed, or an empty string if it
        /// succeeded.
        QString errorString() const;

        /// Returns the arguments of hello-updater for the replacement of the installation
        /// directory after the process \a pid exits, with the log \a log. If \a launch is not
        /// empty, hello-updater starts that program afterwards.
        QStringList updaterArguments(qint64 pid, const QString &log, const QString &launch) const;

        /// Copies the program \a updater into \a directory as updaterFileName(), replacing an
        /// existing copy.
        ///
        /// \return the path of the copy, or an empty string if the copy failed
        static QString copyUpdater(const QString &updater, const QString &directory);

        /// Removes newDirectory() and oldDirectory(), which remain after an update that failed
        /// or that hello-updater did not finish.
        void removeRemnants() const;

        /// Returns the messages of hello-updater in the log \a path, or an empty string if the
        /// log does not exist.
        static QString readLog(const QString &path);

    Q_SIGNALS:
        /// Reports the bytes written and the sum of the declared sizes of the archive.
        void progressChanged(qint64 written, qint64 total);

        void finished();

    private:
        void startThread(std::function<void()> work);
        // Copies the bundle of diskImage into newDirectory() on the thread of the copy, and
        // returns the reason of a failure, or an empty string
        QString copyFromImage(const QString &diskImage);
        void stopExtraction();
        void finishExtraction();

        QString m_installDirectory;
        std::unique_ptr<kit::ZipExtractor> m_extractor;
        std::unique_ptr<QThread> m_thread;
        // Counts the extractions, so that the queued end of a stopped extraction is ignored
        int m_generation = 0;
        std::atomic<bool> m_canceled = false;
        // Written on the thread of the extraction, and read after it ends
        QString m_threadError;
        QString m_errorString;
    };

}

#endif // HELLOUTAU_EDITOR_SOFTWARETREEUPDATE_H
