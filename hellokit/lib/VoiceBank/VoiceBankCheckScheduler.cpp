#include "VoiceBankCheckScheduler.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>

#include <hellokit/Support/FileSystemWatcher.h>

namespace hello::kit {

    class VoiceBankCheckScheduler::Impl {
    public:
        explicit Impl(VoiceBankCheckScheduler *q) : q(q), watcher(q), poll(q), sweep(q) {
            QObject::connect(&watcher, &FileSystemWatcher::changed, q,
                             [this](const QStringList &directories, const QStringList &trees) {
                                 Q_EMIT this->q->checkNeeded(directories + trees);
                             });

            // In both cases the watcher no longer reports on the voice bank, and polling takes
            // over. The entire voice bank is reported immediately, because changes up to this
            // point are unknown as well.
            const auto fallBack = [this](const QString &) {
                following = false;
                if (root.isEmpty()) {
                    return;
                }
                if (!poll.isActive()) {
                    poll.start(pollInterval);
                }
                full();
            };
            QObject::connect(&watcher, &FileSystemWatcher::unwatchable, q, fallBack);
            QObject::connect(&watcher, &FileSystemWatcher::rootNotFound, q, fallBack);

            // Confirmed by the watcher, unless it has already reported a failure.
            QObject::connect(&watcher, &FileSystemWatcher::ready, q,
                             [this] { following = !root.isEmpty() && !poll.isActive(); });

            QObject::connect(&poll, &QTimer::timeout, q, [this] { full(); });
            QObject::connect(&sweep, &QTimer::timeout, q, [this] { full(); });
        }

        VoiceBankCheckScheduler *q;

        // File events are not enabled. VoiceBank::checkDisk() examines directories, and a
        // directory report is available on every system, including macOS.
        FileSystemWatcher watcher;
        QTimer poll;
        QTimer sweep;
        QString root;
        int pollInterval = 5000;
        int sweepInterval = 60000;
        bool following = false;

        void full() {
            if (!root.isEmpty()) {
                Q_EMIT q->checkNeeded({root});
            }
        }

        void restartSweep() {
            sweep.stop();
            if (!root.isEmpty() && sweepInterval > 0) {
                sweep.start(sweepInterval);
            }
        }
    };

    VoiceBankCheckScheduler::VoiceBankCheckScheduler(QObject *parent)
        : QObject(parent), m_impl(std::make_unique<Impl>(this)) {
    }

    VoiceBankCheckScheduler::~VoiceBankCheckScheduler() = default;

    void VoiceBankCheckScheduler::setWatcherProgram(const QString &program) {
        m_impl->watcher.setProgram(program);
    }

    void VoiceBankCheckScheduler::setRoot(const QString &root) {
        auto &impl = *m_impl;
        impl.poll.stop();
        impl.following = false;
        impl.root =
            root.isEmpty()
                ? QString()
                : QDir::cleanPath(QDir::fromNativeSeparators(QFileInfo(root).absoluteFilePath()));
        impl.watcher.setRoots(impl.root.isEmpty() ? QStringList() : QStringList{impl.root});
        impl.restartSweep();
    }

    QString VoiceBankCheckScheduler::root() const {
        return m_impl->root;
    }

    void VoiceBankCheckScheduler::setPollInterval(int milliseconds) {
        m_impl->pollInterval = milliseconds;
        if (m_impl->poll.isActive()) {
            m_impl->poll.start(milliseconds);
        }
    }

    int VoiceBankCheckScheduler::pollInterval() const {
        return m_impl->pollInterval;
    }

    void VoiceBankCheckScheduler::setSweepInterval(int milliseconds) {
        m_impl->sweepInterval = milliseconds;
        m_impl->restartSweep();
    }

    int VoiceBankCheckScheduler::sweepInterval() const {
        return m_impl->sweepInterval;
    }

    void VoiceBankCheckScheduler::setDelay(int milliseconds) {
        m_impl->watcher.setDelay(milliseconds);
    }

    bool VoiceBankCheckScheduler::isFollowing() const {
        return m_impl->following;
    }

    void VoiceBankCheckScheduler::requestFull() {
        m_impl->full();
    }

}
