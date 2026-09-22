#ifndef HELLOKIT_VOICEBANK_VOICEBANKCHECKSCHEDULER_H
#define HELLOKIT_VOICEBANK_VOICEBANKCHECKSCHEDULER_H

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>

namespace hello::kit {

    /// Says when a voice bank has to be compared with the disk, and where.
    ///
    /// It holds no bank. It emits checkNeeded() , and whoever holds the bank passes the
    /// places to VoiceBank::checkDisk() and decides what to do with what that finds. What it
    /// adds is that **a lost notification is never the end of it**: the notifications are one
    /// reason to look among several, and the others do not depend on them.
    ///
    /// **Made for as long as the bank is being edited, and not otherwise.** A project maps its
    /// notes to a bank once, the way UTAU does, and what happens to the bank on disk later is
    /// no concern of a project that is only open. It is the editor of the bank that has to
    /// know, since it is showing what the disk no longer holds.
    ///
    /// - What FileSystemWatcher reports is passed on as it comes.
    /// - Where the watcher cannot follow the bank, on a network share, without its program, or
    ///   once the root has gone, the whole bank is named on a timer instead. See
    ///   setPollInterval() .
    /// - **Whatever the watcher does, the whole bank is named on a slower timer as well**, see
    ///   setSweepInterval() . A notification the system dropped without a word, which no
    ///   watcher can know of, is found by that at the latest.
    /// - requestFull() names it at once, for a caller that knows a moment when it has to be
    ///   right, such as when the window comes back to the front.
    ///
    /// All of these go by stamps. A user who asks for everything to be read again, whatever
    /// the stamps say, is asking for VoiceBank::reloadAllFromDisk() .
    ///
    /// Naming the whole bank costs a listing per directory and no reading, see
    /// VoiceBankDirectoryStamp , which is why doing it on a timer is affordable.
    class HELLOKIT_VOICEBANK_EXPORT VoiceBankCheckScheduler : public QObject {
        Q_OBJECT
    public:
        explicit VoiceBankCheckScheduler(QObject *parent = nullptr);
        ~VoiceBankCheckScheduler() override;

        /// The program that follows the disk, see FileSystemWatcher::setProgram() . Set before
        /// setRoot() .
        void setWatcherProgram(const QString &program);

        /// The bank to schedule for, which starts following it. Empty stops.
        void setRoot(const QString &root);
        QString root() const;

        /// How often the whole bank is named where the watcher cannot follow it, in
        /// milliseconds. 5000 unless set.
        void setPollInterval(int milliseconds);
        int pollInterval() const;

        /// How often the whole bank is named whatever the watcher does, in milliseconds. 60000
        /// unless set, and 0 turns it off, which only a test should do.
        void setSweepInterval(int milliseconds);
        int sweepInterval() const;

        /// How long the watcher gathers what it hears before passing it on, see
        /// FileSystemWatcher::setDelay() .
        void setDelay(int milliseconds);

        /// Whether the watcher follows the bank: it has confirmed it does, and has not said
        /// since that it cannot. A change made before this is true may not be reported, and
        /// is left to the sweep.
        bool isFollowing() const;

    public Q_SLOTS:
        /// Names the whole bank now.
        void requestFull();

    Q_SIGNALS:
        /// \a places may have changed, and are to be passed to VoiceBank::checkDisk() . Absolute
        /// paths with \c / as the separator. The root among them means the whole bank.
        void checkNeeded(const QStringList &places);

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

}

#endif // HELLOKIT_VOICEBANK_VOICEBANKCHECKSCHEDULER_H
