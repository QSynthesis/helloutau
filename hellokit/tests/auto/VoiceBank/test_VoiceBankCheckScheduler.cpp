#include <memory>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <hellokit/VoiceBank/VoiceBank.h>
#include <hellokit/VoiceBank/VoiceBankCheckScheduler.h>

using namespace hello::kit;

namespace fs = std::filesystem;

// Where hello-fswatcher has a way to follow the disk. Anywhere else it answers every root as
// unwatchable, and what depends on it being followed cannot be seen.
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
static constexpr bool followsHere = true;
#else
static constexpr bool followsHere = false;
#endif

class test_VoiceBankCheckScheduler : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

    QString root() const {
        return m_dir->path() + QStringLiteral("/bank");
    }

    static void write(const QString &path, const QByteArray &bytes) {
        QVERIFY(QDir().mkpath(QFileInfo(path).path()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(bytes);
    }

    static void touch(const QString &path) {
        write(path, "RIFF");
    }

    /// Whether the whole bank was named in \a spy .
    bool namedWhole(const QSignalSpy &spy) const {
        for (const auto &arguments : spy) {
            if (arguments.at(0).toStringList().contains(root())) {
                return true;
            }
        }
        return false;
    }

    std::unique_ptr<VoiceBankCheckScheduler> scheduler(int sweep = 0) {
        auto out = std::make_unique<VoiceBankCheckScheduler>();
        out->setDelay(50);
        out->setPollInterval(100);
        out->setSweepInterval(sweep);
        return out;
    }

    /// A bank kept in step with its disk the way its editor would, with a user who says yes to
    /// every reload.
    ///
    /// The sweep is off, so that whatever reaches the bank came by the watcher.
    struct Followed {
        FixedCharsetSelector selector{QStringLiteral("UTF-8")};
        DiagnosticList diagnostics;
        std::optional<VoiceBank> bank;
        std::unique_ptr<VoiceBankCheckScheduler> schedule;
    };

    std::unique_ptr<Followed> followBank() {
        auto out = std::make_unique<Followed>();
        out->bank = VoiceBank::open(root().toStdU16String(), &out->selector, out->diagnostics);
        out->schedule = scheduler();
        auto *followed = out.get();
        connect(out->schedule.get(), &VoiceBankCheckScheduler::checkNeeded, this,
                [followed](const QStringList &places) {
                    QList<fs::path> paths;
                    for (const auto &place : places) {
                        paths += fs::path(place.toStdU16String());
                    }
                    auto &bank = *followed->bank;
                    bank.reloadFromDisk(bank.checkDisk(paths), &followed->selector,
                                        followed->diagnostics);
                });
        out->schedule->setRoot(root());
        return out;
    }

    static QString directoryOf(const VoiceBank &bank, const VoiceSample &sample) {
        return QString::fromStdU16String(bank.directories().at(sample.directory).path.u16string());
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        touch(root() + QStringLiteral("/a.wav"));
    }

    void cleanup() {
        m_dir.reset();
    }

    // Put together the way an editor of a bank would, and the bank is what is looked at: what
    // happened on disk has to end up in it, whatever came in between.
    void a_sample_added_on_disk_reaches_the_bank() {
        if (!followsHere) {
            QSKIP("The watcher program follows nothing on this system.");
        }
        const auto followed = followBank();
        QVERIFY(followed->bank.has_value());
        QTRY_VERIFY_WITH_TIMEOUT(followed->schedule->isFollowing(), 5000);

        touch(root() + QStringLiteral("/deep/down/ka.wav"));
        QTRY_VERIFY_WITH_TIMEOUT(followed->bank->find(60, QStringLiteral("ka")), 5000);
    }

    void an_oto_ini_written_on_disk_reaches_the_bank() {
        if (!followsHere) {
            QSKIP("The watcher program follows nothing on this system.");
        }
        const auto followed = followBank();
        QVERIFY(followed->bank.has_value());
        QTRY_VERIFY_WITH_TIMEOUT(followed->schedule->isFollowing(), 5000);

        write(root() + QStringLiteral("/oto.ini"), "a.wav=named,1,2,3,4,5\r\n");
        QTRY_VERIFY_WITH_TIMEOUT(followed->bank->find(60, QStringLiteral("named")), 5000);
    }

    // What was in it is found where it is now, and not where it was.
    void a_directory_renamed_on_disk_reaches_the_bank() {
        if (!followsHere) {
            QSKIP("The watcher program follows nothing on this system.");
        }
        write(root() + QStringLiteral("/old/oto.ini"), "ka.wav=ka,1,2,3,4,5\r\n");
        touch(root() + QStringLiteral("/old/ka.wav"));
        const auto followed = followBank();
        QVERIFY(followed->bank.has_value());
        QTRY_VERIFY_WITH_TIMEOUT(followed->schedule->isFollowing(), 5000);

        QVERIFY(QDir().rename(root() + QStringLiteral("/old"), root() + QStringLiteral("/new")));
        QTRY_VERIFY_WITH_TIMEOUT(
            [&] {
                const auto *sample = followed->bank->find(60, QStringLiteral("ka"));
                return sample && directoryOf(*followed->bank, *sample) == QStringLiteral("new");
            }(),
            5000);
    }

    // With everything in it, however deep, and with what is written into it afterwards.
    void a_tree_moved_in_on_disk_reaches_the_bank() {
        if (!followsHere) {
            QSKIP("The watcher program follows nothing on this system.");
        }
        const QString outside = m_dir->path() + QStringLiteral("/elsewhere");
        write(outside + QStringLiteral("/deep/oto.ini"), "ka.wav=ka,1,2,3,4,5\r\n");
        touch(outside + QStringLiteral("/deep/ka.wav"));
        const auto followed = followBank();
        QVERIFY(followed->bank.has_value());
        QTRY_VERIFY_WITH_TIMEOUT(followed->schedule->isFollowing(), 5000);

        QVERIFY(QDir().rename(outside, root() + QStringLiteral("/moved")));
        QTRY_VERIFY_WITH_TIMEOUT(followed->bank->find(60, QStringLiteral("ka")), 5000);

        touch(root() + QStringLiteral("/moved/deep/ki.wav"));
        QTRY_VERIFY_WITH_TIMEOUT(followed->bank->find(60, QStringLiteral("ki")), 5000);
    }

    void a_directory_removed_on_disk_reaches_the_bank() {
        if (!followsHere) {
            QSKIP("The watcher program follows nothing on this system.");
        }
        touch(root() + QStringLiteral("/sub/ka.wav"));
        const auto followed = followBank();
        QVERIFY(followed->bank.has_value());
        QVERIFY(followed->bank->find(60, QStringLiteral("ka")));
        QTRY_VERIFY_WITH_TIMEOUT(followed->schedule->isFollowing(), 5000);

        QVERIFY(QDir(root() + QStringLiteral("/sub")).removeRecursively());
        QTRY_VERIFY_WITH_TIMEOUT(!followed->bank->find(60, QStringLiteral("ka")), 5000);
        QVERIFY(followed->bank->find(60, QStringLiteral("a")));
    }

    // What no watcher can know of, a notification the system dropped, is found by the sweep.
    // So the sweep runs whatever the watcher does.
    void the_whole_bank_is_named_now_and_then_while_the_watcher_follows() {
        if (!followsHere) {
            QSKIP("The watcher program follows nothing on this system.");
        }
        const auto schedule = scheduler(200);
        QSignalSpy spy(schedule.get(), &VoiceBankCheckScheduler::checkNeeded);
        schedule->setRoot(root());
        QTRY_VERIFY_WITH_TIMEOUT(schedule->isFollowing(), 5000);
        spy.clear();
        QTRY_VERIFY_WITH_TIMEOUT(namedWhole(spy), 5000);
        QVERIFY(schedule->isFollowing());
    }

    // Without a watcher, the bank is looked at on a timer instead, and at once.
    void without_a_watcher_the_bank_is_polled() {
        const auto schedule = scheduler();
        schedule->setWatcherProgram(m_dir->path() + QStringLiteral("/no such program.exe"));
        QSignalSpy spy(schedule.get(), &VoiceBankCheckScheduler::checkNeeded);
        schedule->setRoot(root());
        QTRY_VERIFY_WITH_TIMEOUT(spy.size() >= 3, 5000);
        QVERIFY(namedWhole(spy));
        QVERIFY(!schedule->isFollowing());
    }

    // A root that went, and may come back, is looked at on a timer as well.
    void a_root_that_goes_is_polled() {
        if (!followsHere) {
            QSKIP("The watcher program follows nothing on this system.");
        }
        const auto schedule = scheduler();
        schedule->setRoot(root());
        QTRY_VERIFY_WITH_TIMEOUT(schedule->isFollowing(), 5000);

        QSignalSpy spy(schedule.get(), &VoiceBankCheckScheduler::checkNeeded);
        QVERIFY(QDir().rename(root(), root() + QStringLiteral(" moved")));
        QTRY_VERIFY_WITH_TIMEOUT(spy.size() >= 3, 5000);
        QVERIFY(!schedule->isFollowing());
    }

    void asking_names_the_whole_bank_at_once() {
        const auto schedule = scheduler();
        schedule->setRoot(root());
        QSignalSpy spy(schedule.get(), &VoiceBankCheckScheduler::checkNeeded);
        schedule->requestFull();
        QCOMPARE(spy.size(), 1);
        QCOMPARE(spy.at(0).at(0).toStringList(), QStringList{root()});
    }
};

QTEST_GUILESS_MAIN(test_VoiceBankCheckScheduler)

#include "test_VoiceBankCheckScheduler.moc"
