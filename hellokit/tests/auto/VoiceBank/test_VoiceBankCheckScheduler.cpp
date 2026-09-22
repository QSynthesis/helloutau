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

    static void touch(const QString &path) {
        QVERIFY(QDir().mkpath(QFileInfo(path).path()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("RIFF");
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

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        touch(root() + QStringLiteral("/a.wav"));
    }

    void cleanup() {
        m_dir.reset();
    }

    // Put together the way a holder of a bank would: a new sample on disk is in the bank soon
    // after, and nobody called anything by hand.
    void a_sample_added_on_disk_reaches_the_bank() {
#ifndef Q_OS_WIN
        QSKIP("The watcher program follows nothing yet on this system.");
#endif
        FixedCharsetSelector selector(QStringLiteral("UTF-8"));
        DiagnosticList diagnostics;
        auto bank = VoiceBank::open(root().toStdU16String(), &selector, diagnostics);
        QVERIFY(bank.has_value());

        const auto schedule = scheduler();
        connect(schedule.get(), &VoiceBankCheckScheduler::checkNeeded, this,
                [&](const QStringList &places) {
                    QList<fs::path> paths;
                    for (const auto &place : places) {
                        paths += fs::path(place.toStdU16String());
                    }
                    bank->reloadFromDisk(bank->checkDisk(paths), &selector, diagnostics);
                });
        schedule->setRoot(root());
        QTRY_VERIFY_WITH_TIMEOUT(schedule->isFollowing(), 5000);

        touch(root() + QStringLiteral("/deep/down/ka.wav"));
        QTRY_VERIFY_WITH_TIMEOUT(bank->find(60, QStringLiteral("ka")), 5000);
    }

    // What no watcher can know of, a notification the system dropped, is found by the sweep.
    // So the sweep runs whatever the watcher does.
    void the_whole_bank_is_named_now_and_then_while_the_watcher_follows() {
#ifndef Q_OS_WIN
        QSKIP("The watcher program follows nothing yet on this system.");
#endif
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
#ifndef Q_OS_WIN
        QSKIP("The watcher program follows nothing yet on this system.");
#endif
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
