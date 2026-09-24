#ifndef HELLOKIT_VOICEBANK_VOICEBANKCHECKSCHEDULER_H
#define HELLOKIT_VOICEBANK_VOICEBANKCHECKSCHEDULER_H

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>

namespace hello::kit {

    /// Determines when and where a voice bank must be compared with the disk.
    ///
    /// The scheduler holds no voice bank. It emits checkNeeded(), and the owner of the voice
    /// bank passes the reported locations to VoiceBankDiskState::checkDisk() and handles the
    /// result. Its purpose is that **a lost notification never causes a change to be missed
    /// permanently**. File system notifications are one of several triggers, and the other triggers
    /// do not depend on them.
    ///
    /// **Intended for use only while a voice bank is being edited.** A project maps its notes to
    /// a voice bank once, as UTAU does, and later changes on disk do not concern a project that
    /// is merely open. Only the voice bank editor must be informed, because it displays content
    /// that may no longer match the disk.
    ///
    /// - Reports from FileSystemWatcher are forwarded as they arrive.
    /// - If the watcher cannot monitor the voice bank, for example on a network share, when
    ///   the monitor program is unavailable or after the root has been removed, the entire voice
    ///   bank is reported periodically instead. See setPollInterval().
    /// - **Regardless of the watcher state, the entire voice bank is also reported at a longer
    ///   interval.** See setSweepInterval(). A notification that the system dropped silently,
    ///   which no watcher can detect, is found by this sweep at the latest.
    /// - requestFull() reports the entire voice bank immediately, for callers that require an
    ///   up-to-date state at a specific moment, such as when the window is reactivated.
    ///
    /// All of these checks compare stamps. A user request to reread everything regardless of
    /// the stamps corresponds to VoiceBank::reloadAllFromDisk().
    ///
    /// Checking the entire voice bank costs one listing per directory and no file reads (see
    /// VoiceBankDirectoryStamp), which makes periodic checks affordable.
    class HELLOKIT_VOICEBANK_EXPORT VoiceBankCheckScheduler : public QObject {
        Q_OBJECT
    public:
        explicit VoiceBankCheckScheduler(QObject *parent = nullptr);
        ~VoiceBankCheckScheduler() override;

        /// The monitor program. See FileSystemWatcher::setProgram(). Must be set before
        /// setRoot().
        void setWatcherProgram(const QString &program);

        /// Sets the root of the voice bank and starts monitoring it. An empty path stops
        /// monitoring.
        void setRoot(const QString &root);
        QString root() const;

        /// The interval in milliseconds at which the entire voice bank is reported while the
        /// watcher cannot monitor it. Defaults to 5000.
        void setPollInterval(int milliseconds);
        int pollInterval() const;

        /// The interval in milliseconds at which the entire voice bank is reported regardless of
        /// the watcher state. Defaults to 60000. A value of 0 disables the sweep and is intended
        /// for tests only.
        void setSweepInterval(int milliseconds);
        int sweepInterval() const;

        /// The coalescing interval of the watcher. See FileSystemWatcher::setDelay().
        void setDelay(int milliseconds);

        /// Returns whether the watcher monitors the voice bank, that is, whether it has
        /// confirmed monitoring and has reported no failure since. Changes made before this
        /// becomes true may go unreported and are left to the sweep.
        bool isFollowing() const;

    public Q_SLOTS:
        /// Reports the entire voice bank immediately.
        void requestFull();

    Q_SIGNALS:
        /// \a places may have changed and are to be passed to VoiceBankDiskState::checkDisk(). The
        /// paths are absolute and use \c / as the separator. The root, if present, denotes the
        /// entire voice bank.
        void checkNeeded(const QStringList &places);

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOKIT_VOICEBANK_VOICEBANKCHECKSCHEDULER_H
