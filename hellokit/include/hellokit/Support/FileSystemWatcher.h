#ifndef HELLOKIT_SUPPORT_FILESYSTEMWATCHER_H
#define HELLOKIT_SUPPORT_FILESYSTEMWATCHER_H

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Support/HelloKitSupportGlobal.h>

namespace hello::kit {

    /// Says which directories under a set of roots may have changed on disk.
    ///
    /// **Not QFileSystemWatcher, and not like it.** That one registers every path on its own,
    /// follows nothing below them, and on Windows holds each directory it follows open, which
    /// keeps a user from renaming or removing any of them while this program runs. A voice bank
    /// is a tree its author reorganizes, so this follows whole trees and holds none of them.
    ///
    /// **What it says is a hint, not a fact.** A directory named here may not have changed at
    /// all, and a change can be missed where the system loses events. So what it names is to be
    /// looked at and compared with what was there, never taken as the change itself. Where it
    /// knows it lost track, it says so by naming the whole root.
    ///
    /// The following is done by \c hello-fswatcher , a process of its own that ships beside the
    /// libraries, so that whatever the system's notifications do, a crash, a hang or a limit run
    /// into, stays out of the editor. It is restarted when it dies, and every root is named once
    /// it is back, since what happened in between is not known. After a few deaths in a row it
    /// is given up on, and every root is reported as unwatchable().
    ///
    /// \note Paths come out with \c / as the separator, each starting with one of roots()
    ///       exactly as it reads there.
    ///
    /// \note On Windows, macOS and Linux, which are the systems this project builds for.
    class HELLOKIT_SUPPORT_EXPORT FileSystemWatcher : public QObject {
        Q_OBJECT
    public:
        explicit FileSystemWatcher(QObject *parent = nullptr);
        ~FileSystemWatcher() override;

        /// Where the program is looked for unless setProgram() says otherwise: \c hello-fswatcher
        /// beside the application.
        static QString defaultProgram();

        /// The program that follows the disk, which is started and spoken to over its standard
        /// input and output.
        ///
        /// **Set before setRoots()** , which is what starts it. One already running goes
        /// on running, and this one is started only the next time one is: after a death, or
        /// after the roots were emptied and set again.
        ///
        /// \warning Never a path that came from a project, a voice bank or anything else a user
        ///          was handed. It is started without asking.
        void setProgram(const QString &program);
        QString program() const;

        /// Replaces what is followed, and **starts the program** where none is running yet, so
        /// setProgram() comes first. Empty stops it.
        ///
        /// ready() says when the new roots are followed. A change made before it may go
        /// unreported.
        void setRoots(const QStringList &roots);
        QStringList roots() const;

        /// How long to gather what the program says before changed() is emitted, in
        /// milliseconds. One copy of five hundred files is then one signal rather than five
        /// hundred. 300 unless set.
        void setDelay(int milliseconds);
        int delay() const;

        /// The program's process, or 0 where none runs. For tests and for diagnostics.
        qint64 processId() const;

    Q_SIGNALS:
        /// \param directories what is directly in each may have changed: its listing, or a file
        ///        in it
        /// \param trees each, and everything under it, may have changed. A directory under one
        ///        of these is not named again in \a directories .
        void changed(const QStringList &directories, const QStringList &trees);

        /// \a root is not there any more, or never was.
        void rootGone(const QString &root);

        /// Nothing will be said about \a root , and it has to be looked at some other way.
        void unwatchable(const QString &root);

        /// The roots last set are followed, and a change made from now on is reported.
        void ready();

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

}

#endif // HELLOKIT_SUPPORT_FILESYSTEMWATCHER_H
