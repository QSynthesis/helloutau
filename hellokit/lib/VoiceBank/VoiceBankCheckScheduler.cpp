#include "VoiceBankCheckScheduler.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>

#include <stdcorelib/pimpl.h>

#include <hellokit/Support/FileSystemWatcher.h>

namespace hello::kit {

    class VoiceBankCheckScheduler::Impl {
    public:
        using Decl = VoiceBankCheckScheduler;

        explicit Impl(Decl *decl) : _decl(decl), watcher(decl), poll(decl), sweep(decl) {
            QObject::connect(&watcher, &FileSystemWatcher::changed, decl,
                             [this](const QStringList &directories, const QStringList &trees) {
                                 stdc_decl_t;
                                 Q_EMIT decl.checkNeeded(directories + trees);
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
            QObject::connect(&watcher, &FileSystemWatcher::unwatchable, decl, fallBack);
            QObject::connect(&watcher, &FileSystemWatcher::rootNotFound, decl, fallBack);

            // Confirmed by the watcher, unless it has already reported a failure.
            QObject::connect(&watcher, &FileSystemWatcher::ready, decl,
                             [this] { following = !root.isEmpty() && !poll.isActive(); });

            QObject::connect(&poll, &QTimer::timeout, decl, [this] { full(); });
            QObject::connect(&sweep, &QTimer::timeout, decl, [this] { full(); });
        }

        Decl *_decl;

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
            stdc_decl_t;
            if (!root.isEmpty()) {
                Q_EMIT decl.checkNeeded({root});
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
        : QObject(parent), _impl(std::make_unique<Impl>(this)) {
    }

    VoiceBankCheckScheduler::~VoiceBankCheckScheduler() = default;

    void VoiceBankCheckScheduler::setWatcherProgram(const QString &program) {
        stdc_impl_t;
        impl.watcher.setProgram(program);
    }

    void VoiceBankCheckScheduler::setRoot(const QString &root) {
        stdc_impl_t;
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
        stdc_impl_t;
        return impl.root;
    }

    void VoiceBankCheckScheduler::setPollInterval(int milliseconds) {
        stdc_impl_t;
        impl.pollInterval = milliseconds;
        if (impl.poll.isActive()) {
            impl.poll.start(milliseconds);
        }
    }

    int VoiceBankCheckScheduler::pollInterval() const {
        stdc_impl_t;
        return impl.pollInterval;
    }

    void VoiceBankCheckScheduler::setSweepInterval(int milliseconds) {
        stdc_impl_t;
        impl.sweepInterval = milliseconds;
        impl.restartSweep();
    }

    int VoiceBankCheckScheduler::sweepInterval() const {
        stdc_impl_t;
        return impl.sweepInterval;
    }

    void VoiceBankCheckScheduler::setDelay(int milliseconds) {
        stdc_impl_t;
        impl.watcher.setDelay(milliseconds);
    }

    bool VoiceBankCheckScheduler::isFollowing() const {
        stdc_impl_t;
        return impl.following;
    }

    void VoiceBankCheckScheduler::requestFull() {
        stdc_impl_t;
        impl.full();
    }

}
