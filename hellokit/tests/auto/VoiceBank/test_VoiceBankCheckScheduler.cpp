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

    /// Returns whether \a spy recorded a report of the entire voice bank.
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

    /// A voice bank kept synchronized with the disk as its editor would keep it, with a user
    /// who accepts every reload.
    ///
    /// The sweep is disabled, so that every change reaching the voice bank arrives through the
    /// watcher.
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

    // Assembled as a voice bank editor would assemble it, and the voice bank is what is
    // verified: every change on disk must reach it, regardless of the intermediate steps.
    void a_sample_added_on_disk_reaches_the_bank() {
        const auto followed = followBank();
        QVERIFY(followed->bank.has_value());
        QTRY_VERIFY_WITH_TIMEOUT(followed->schedule->isFollowing(), 5000);

        touch(root() + QStringLiteral("/deep/down/ka.wav"));
        QTRY_VERIFY_WITH_TIMEOUT(followed->bank->find(60, QStringLiteral("ka")), 5000);
    }

    void an_oto_ini_written_on_disk_reaches_the_bank() {
        const auto followed = followBank();
        QVERIFY(followed->bank.has_value());
        QTRY_VERIFY_WITH_TIMEOUT(followed->schedule->isFollowing(), 5000);

        write(root() + QStringLiteral("/oto.ini"), "a.wav=named,1,2,3,4,5\r\n");
        QTRY_VERIFY_WITH_TIMEOUT(followed->bank->find(60, QStringLiteral("named")), 5000);
    }

    // The contents of a renamed directory are found at the new location, not the old one.
    void a_directory_renamed_on_disk_reaches_the_bank() {
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

    // Including its entire subtree at any depth, and files written into it afterward.
    void a_tree_moved_in_on_disk_reaches_the_bank() {
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
        touch(root() + QStringLiteral("/sub/ka.wav"));
        const auto followed = followBank();
        QVERIFY(followed->bank.has_value());
        QVERIFY(followed->bank->find(60, QStringLiteral("ka")));
        QTRY_VERIFY_WITH_TIMEOUT(followed->schedule->isFollowing(), 5000);

        QVERIFY(QDir(root() + QStringLiteral("/sub")).removeRecursively());
        QTRY_VERIFY_WITH_TIMEOUT(!followed->bank->find(60, QStringLiteral("ka")), 5000);
        QVERIFY(followed->bank->find(60, QStringLiteral("a")));
    }

    // A notification dropped by the system, which no watcher can detect, is found by the sweep.
    // The sweep therefore runs regardless of the watcher state.
    void the_whole_bank_is_named_now_and_then_while_the_watcher_follows() {
        const auto schedule = scheduler(200);
        QSignalSpy spy(schedule.get(), &VoiceBankCheckScheduler::checkNeeded);
        schedule->setRoot(root());
        QTRY_VERIFY_WITH_TIMEOUT(schedule->isFollowing(), 5000);
        spy.clear();
        QTRY_VERIFY_WITH_TIMEOUT(namedWhole(spy), 5000);
        QVERIFY(schedule->isFollowing());
    }

    // Without a watcher, the voice bank is polled instead, starting immediately.
    void without_a_watcher_the_bank_is_polled() {
        const auto schedule = scheduler();
        schedule->setWatcherProgram(m_dir->path() + QStringLiteral("/no such program.exe"));
        QSignalSpy spy(schedule.get(), &VoiceBankCheckScheduler::checkNeeded);
        schedule->setRoot(root());
        QTRY_VERIFY_WITH_TIMEOUT(spy.size() >= 3, 5000);
        QVERIFY(namedWhole(spy));
        QVERIFY(!schedule->isFollowing());
    }

    // A removed root, which may be restored, is also polled.
    void a_root_that_goes_is_polled() {
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
