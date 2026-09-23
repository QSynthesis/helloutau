#ifndef HELLOKIT_SUPPORT_FILESYSTEMWATCHER_H
#define HELLOKIT_SUPPORT_FILESYSTEMWATCHER_H

#include <memory>

#include <QtCore/QList>
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
    /// **File events** report individual entries in addition to directories. They are disabled
    /// by default and unavailable on macOS, where FSEvents reports directories only. See
    /// setFileEventsEnabled().
    ///
    /// \note Reported paths use \c / as the separator and begin with one of roots() verbatim.
    ///
    /// \note Supported on Windows, macOS and Linux, the platforms this project targets.
    class HELLOKIT_SUPPORT_EXPORT FileSystemWatcher : public QObject {
        Q_OBJECT
    public:
        /// A change of one entry, reported by fileEvents().
        struct FileEvent {
            enum Type {
                Created, ///< appeared, by creation or by a rename or move into place
                Deleted, ///< disappeared, by deletion or by a rename or move away
                Changed, ///< the contents or metadata may have changed
            };

            Type type = Changed;
            QString path;

            bool operator==(const FileEvent &other) const {
                return type == other.type && path == other.path;
            }
        };

        explicit FileSystemWatcher(QObject *parent = nullptr);
        ~FileSystemWatcher() override;

        /// Returns whether this system reports changes per entry, which file events require.
        /// True on Windows and Linux, false on macOS.
        static bool fileEventsAvailable();

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

        /// Whether fileEvents() is emitted in addition to changed(). Disabled by default.
        ///
        /// **Must be set before setRoots()**, like setProgram(). The setting takes effect at the
        /// next start of the program. Ignored if fileEventsAvailable() returns false, because
        /// the program rejects the request on such a system.
        void setFileEventsEnabled(bool enabled);
        bool fileEventsEnabled() const;

        /// The process ID of the program, or 0 if it is not running. Intended for tests and
        /// diagnostics.
        qint64 processId() const;

    Q_SIGNALS:
        /// \param directories directories whose direct contents may have changed, either the
        ///        listing or a file in it
        /// \param trees directories whose entire subtree may have changed. Directories within
        ///        these trees are not repeated in \a directories .
        void changed(const QStringList &directories, const QStringList &trees);

        /// Emitted only if file events are enabled, immediately before changed() for the same
        /// interval. The directory of every entry is also reported by changed().
        ///
        /// \param events the entry changes of the interval in the order reported, neither merged
        ///        nor deduplicated, because a creation followed by a deletion differs from a
        ///        deletion followed by a creation
        ///
        /// Like changed(), these are hints. After events were lost or the program was restarted,
        /// the affected trees are reported by changed() only.
        void fileEvents(const QList<hello::kit::FileSystemWatcher::FileEvent> &events);

        /// \a root does not exist, either because it was removed or because it never existed.
        void rootNotFound(const QString &root);

        /// \a root is no longer monitored, and changes to it must be detected by other means.
        void unwatchable(const QString &root);

        /// The most recently set roots are monitored, and subsequent changes are reported.
        void ready();

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOKIT_SUPPORT_FILESYSTEMWATCHER_H
