#ifndef HELLOKIT_SUPPORT_FILESYSTEMWATCHER_H
#define HELLOKIT_SUPPORT_FILESYSTEMWATCHER_H

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Support/HelloKitSupportGlobal.h>

namespace hello::kit {

    /// Reports directories under a set of roots that may have changed on disk.
    ///
    /// **Unlike QFileSystemWatcher.** QFileSystemWatcher registers each path individually, does
    /// not monitor subdirectories, and on Windows keeps every monitored directory open, which
    /// prevents the user from renaming or deleting it while the application runs. A voice bank
    /// is a directory tree that its author reorganizes, so this class monitors entire trees and
    /// keeps no directory open.
    ///
    /// **Reports are hints, not facts.** A reported directory may be unchanged, and a change may
    /// go unreported when the system drops events. A reported directory must therefore be
    /// rescanned and compared with its previous state, never treated as the change itself. When
    /// event loss is detected, the entire root is reported.
    ///
    /// Monitoring is performed by \c hello-fswatcher , a separate process shipped with the
    /// libraries. This isolates the editor from failures of the system notification facilities,
    /// such as crashes, hangs and resource limits. The process is restarted after an unexpected
    /// exit, and every root is then reported once, because changes during the interruption are
    /// unknown. After several consecutive failures, monitoring is abandoned and every root is
    /// reported through unwatchable().
    ///
    /// \note Reported paths use \c / as the separator and begin with one of roots() verbatim.
    ///
    /// \note Supported on Windows, macOS and Linux, the platforms this project targets.
    class HELLOKIT_SUPPORT_EXPORT FileSystemWatcher : public QObject {
        Q_OBJECT
    public:
        explicit FileSystemWatcher(QObject *parent = nullptr);
        ~FileSystemWatcher() override;

        /// The monitor program used unless setProgram() specifies another: \c hello-fswatcher in
        /// the application directory.
        static QString defaultProgram();

        /// The monitor program, which communicates over its standard input and output.
        ///
        /// **Must be set before setRoots()**, which starts the program. A running instance is
        /// not replaced. The new program takes effect at the next start, which occurs after an
        /// unexpected exit or after the roots are cleared and set again.
        ///
        /// \warning Never pass a path obtained from a project, a voice bank or any other
        ///          user-supplied data. The program is started without confirmation.
        void setProgram(const QString &program);
        QString program() const;

        /// Replaces the monitored roots and **starts the program** if it is not running, so
        /// setProgram() must be called first. An empty list stops the program.
        ///
        /// ready() is emitted once the new roots are monitored. Changes made before that may go
        /// unreported.
        void setRoots(const QStringList &roots);
        QStringList roots() const;

        /// The interval in milliseconds over which reports are coalesced before changed() is
        /// emitted, so that copying five hundred files produces one signal instead of five
        /// hundred. Defaults to 300.
        void setDelay(int milliseconds);
        int delay() const;

        /// The process ID of the program, or 0 if it is not running. Intended for tests and
        /// diagnostics.
        qint64 processId() const;

    Q_SIGNALS:
        /// \param directories directories whose direct contents may have changed, either the
        ///        listing or a file in it
        /// \param trees directories whose entire subtree may have changed. Directories within
        ///        these trees are not repeated in \a directories .
        void changed(const QStringList &directories, const QStringList &trees);

        /// \a root does not exist, either because it was removed or because it never existed.
        void rootGone(const QString &root);

        /// \a root is no longer monitored, and changes to it must be detected by other means.
        void unwatchable(const QString &root);

        /// The most recently set roots are monitored, and subsequent changes are reported.
        void ready();

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

}

#endif // HELLOKIT_SUPPORT_FILESYSTEMWATCHER_H
