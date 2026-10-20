#ifndef HELLOKIT_SUPPORT_SINGLEPROCESSPEER_H
#define HELLOKIT_SUPPORT_SINGLEPROCESSPEER_H

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QStringList>

#include <hellokit/Support/HelloKitSupportGlobal.h>

namespace hello::kit {

    /// One of the processes of an application that share a key, of which one is the primary
    /// process and receives the requests of the others. A later process sends its arguments to
    /// the primary process and ends, as a second start of the application on Windows and Linux
    /// would otherwise open a second process. macOS starts one process per user already, and
    /// passes the files that are opened later to it as QFileOpenEvent.
    ///
    /// The primary process holds a lock file in the temporary directory and listens on a local
    /// server, both named after the key and the home directory of the user. The lock file elects
    /// the primary process, because a local server on Windows is a named pipe, which several
    /// processes can listen on under one name. A lock file whose process has ended is taken over.
    ///
    /// The peer is attached to an application rather than derived from QCoreApplication, and the
    /// application constructs it after the application object.
    class HELLOKIT_SUPPORT_EXPORT SingleProcessPeer : public QObject {
        Q_OBJECT
    public:
        /// Becomes the primary process of \a key if no other process of the same user holds it.
        /// Processes with different keys, such as those with different settings directories, are
        /// independent.
        explicit SingleProcessPeer(const QString &key, QObject *parent = nullptr);
        ~SingleProcessPeer();

        /// Returns whether this process is the primary process, which emits
        /// activateRequested() for the requests of the other processes.
        bool isPrimary() const;

        /// Sends \a arguments and \a workingDirectory to the primary process, waiting up to
        /// \a timeout milliseconds for the primary process to listen and to confirm the receipt.
        /// On Windows the primary process may then bring its window to the foreground, which
        /// Windows otherwise denies a process in the background.
        ///
        /// \return whether the primary process confirmed the receipt. A primary process does not
        ///         send, and returns \c false .
        bool sendToPrimary(const QStringList &arguments, const QString &workingDirectory,
                           int timeout = 5000);

    Q_SIGNALS:
        /// Another process asks the primary process to act on \a arguments , in which a relative
        /// path is relative to \a workingDirectory .
        void activateRequested(const QStringList &arguments, const QString &workingDirectory);

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOKIT_SUPPORT_SINGLEPROCESSPEER_H
