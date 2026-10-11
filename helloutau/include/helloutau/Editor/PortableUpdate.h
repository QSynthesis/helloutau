#ifndef HELLOUTAU_EDITOR_PORTABLEUPDATE_H
#define HELLOUTAU_EDITOR_PORTABLEUPDATE_H

#include <atomic>
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

    /// The update of the portable application: the extraction of the new version into
    /// <tt><installation>.new</tt> beside the installation directory, and the replacement of the
    /// installation directory by \c hello-updater after the application quits. See the update of
    /// the portable application in docs/Distribution.md.
    class HELLOUTAU_EDITOR_EXPORT PortableUpdate : public QObject {
        Q_OBJECT
    public:
        /// Constructs the update of the installation in \a installDirectory, the parent of the
        /// directory of the program.
        explicit PortableUpdate(const QString &installDirectory, QObject *parent = nullptr);

        /// Cancels a running extraction and waits for it to end.
        ~PortableUpdate();

        /// Returns the installation directory of the running application, the parent of the
        /// directory of the program.
        static QString applicationInstallDirectory();

        /// Returns the file name of \c hello-updater, with \c .exe on Windows.
        static QString updaterFileName();

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

        /// Cancels the running extraction without finished(). newDirectory() is removed on the
        /// thread of the extraction once it stops.
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

        /// Copies hello-updater from the directory of the program into \a directory, replacing an
        /// existing copy.
        ///
        /// \return the path of the copy, or an empty string if the copy failed
        static QString copyUpdater(const QString &directory);

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

#endif // HELLOUTAU_EDITOR_PORTABLEUPDATE_H
