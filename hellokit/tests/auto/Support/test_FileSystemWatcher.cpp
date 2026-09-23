#include <algorithm>
#include <memory>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <hellokit/Support/FileSystemWatcher.h>

#ifdef Q_OS_WIN
#  include <windows.h>
#else
#  include <signal.h>
#endif

using namespace hello::kit;

namespace {

    /// All paths reported by changed() since the spy was created, one list per kind.
    struct Changes {
        QStringList directories;
        QStringList trees;
    };

    Changes collect(const QSignalSpy &spy) {
        Changes out;
        for (const auto &arguments : spy) {
            out.directories += arguments.at(0).toStringList();
            out.trees += arguments.at(1).toStringList();
        }
        return out;
    }

    /// Returns whether \a path was reported, either as a directory or within a reported tree.
    ///
    /// A directory within a reported tree is not repeated among the directories. FSEvents also
    /// reports the creation of directories made shortly before monitoring started, as trees, so
    /// the directories created by init() may cover a later report within the same interval.
    bool reported(const Changes &changes, const QString &path) {
        return changes.directories.contains(path) ||
               std::any_of(changes.trees.begin(), changes.trees.end(), [&](const QString &tree) {
                   return path == tree || path.startsWith(tree + QLatin1Char('/'));
               });
    }

}

class test_FileSystemWatcher : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

    // A percent sign in the name, because the protocol escapes it, and one followed by text
    // that resembles an escape sequence, which is misread if escaping is missing.
    QString root() const {
        return m_dir->path() + QStringLiteral("/voice/bank %25 100%");
    }

    QString at(const QString &relative) const {
        return root() + QLatin1Char('/') + relative;
    }

    static void touch(const QString &path) {
        QVERIFY(QDir().mkpath(QFileInfo(path).path()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("RIFF");
    }

    /// A watcher that monitors root() and has emitted ready(). File events keep their default
    /// unless requested, so that the tests of the default observe it.
    std::unique_ptr<FileSystemWatcher> follow(bool fileEvents = false) {
        auto watcher = std::make_unique<FileSystemWatcher>();
        watcher->setDelay(50);
        if (fileEvents) {
            watcher->setFileEventsEnabled(true);
        }
        QSignalSpy ready(watcher.get(), &FileSystemWatcher::ready);
        watcher->setRoots({root()});
        [&] { QVERIFY(ready.wait(10000)); }();
        return watcher;
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        QVERIFY(QDir().mkpath(at(QStringLiteral("a/b"))));
    }

    void cleanup() {
        m_dir.reset();
    }

    void a_file_deep_down_is_reported() {
        const auto watcher = follow();
        QSignalSpy spy(watcher.get(), &FileSystemWatcher::changed);

        touch(at(QStringLiteral("a/b/ka.wav")));
        QTRY_VERIFY_WITH_TIMEOUT(reported(collect(spy), at(QStringLiteral("a/b"))), 5000);
    }

    // The reason for not using QFileSystemWatcher: on Windows it keeps every monitored
    // directory open, which prevents the author of a voice bank from renaming or deleting it.
    void what_is_followed_can_be_renamed_and_removed() {
        QVERIFY(QDir().mkpath(at(QStringLiteral("removed"))));
        QVERIFY(QDir().mkpath(at(QStringLiteral("old"))));
        const auto watcher = follow();
        QSignalSpy notFound(watcher.get(), &FileSystemWatcher::rootNotFound);

        QVERIFY(QDir(at(QStringLiteral("removed"))).removeRecursively());
        QVERIFY(QDir().rename(at(QStringLiteral("old")), at(QStringLiteral("new"))));
        QVERIFY(QDir().rename(root(), root() + QStringLiteral(" renamed")));
        QTRY_COMPARE_WITH_TIMEOUT(notFound.size(), 1, 5000);
        QCOMPARE(notFound.at(0).at(0).toString(), root());

        // The parent directory of the root is restored as well.
        QVERIFY(QDir().rename(root() + QStringLiteral(" renamed"), root()));
        QVERIFY(QDir().rename(m_dir->path() + QStringLiteral("/voice"),
                              m_dir->path() + QStringLiteral("/voice2")));
    }

    // A directory created with content requires examination of its entire subtree, not only of
    // its name.
    void a_directory_moved_in_is_named_as_a_tree() {
        const QString outside = m_dir->path() + QStringLiteral("/elsewhere");
        touch(outside + QStringLiteral("/deep/ka.wav"));
        const auto watcher = follow();
        QSignalSpy spy(watcher.get(), &FileSystemWatcher::changed);

        QVERIFY(QDir().rename(outside, at(QStringLiteral("moved"))));
        QTRY_VERIFY_WITH_TIMEOUT(collect(spy).trees.contains(at(QStringLiteral("moved"))), 5000);
    }

    void what_is_beside_a_root_is_not_reported() {
        const auto watcher = follow();
        QSignalSpy spy(watcher.get(), &FileSystemWatcher::changed);

        touch(m_dir->path() + QStringLiteral("/voice/other/ka.wav"));
        touch(at(QStringLiteral("a/marker.wav")));
        QTRY_VERIFY_WITH_TIMEOUT(reported(collect(spy), at(QStringLiteral("a"))), 5000);

        for (const auto &directory : collect(spy).directories) {
            QVERIFY2(directory.startsWith(root()), qPrintable(directory));
        }
    }

    void a_root_that_does_not_exist_is_not_found() {
        FileSystemWatcher watcher;
        QSignalSpy notFound(&watcher, &FileSystemWatcher::rootNotFound);
        const QString missing = m_dir->path() + QStringLiteral("/nothing here");
        watcher.setRoots({missing});
        QTRY_COMPARE_WITH_TIMEOUT(notFound.size(), 1, 10000);
        QCOMPARE(notFound.at(0).at(0).toString(), missing);
    }

    // Changes during the interruption are unknown, so every root is reported after the restart.
    void a_program_that_dies_is_started_again_and_every_root_named() {
        const auto watcher = follow();
        QSignalSpy spy(watcher.get(), &FileSystemWatcher::changed);
        QSignalSpy ready(watcher.get(), &FileSystemWatcher::ready);

        const qint64 first = watcher->processId();
        QVERIFY(first != 0);
#ifdef Q_OS_WIN
        const HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, DWORD(first));
        QVERIFY(process);
        QVERIFY(TerminateProcess(process, 1));
        CloseHandle(process);
#else
        QCOMPARE(::kill(pid_t(first), SIGKILL), 0);
#endif

        QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(collect(spy).trees.contains(root()), 5000);
        QVERIFY(watcher->processId() != 0);
        QVERIFY(watcher->processId() != first);

        // Monitoring resumes.
        touch(at(QStringLiteral("a/after.wav")));
        QTRY_VERIFY_WITH_TIMEOUT(reported(collect(spy), at(QStringLiteral("a"))), 5000);
    }

    // File events are an addition requested explicitly. The voice bank monitoring relies on
    // changed() alone, so without the request no file event may be emitted.
    void file_events_are_not_reported_unless_enabled() {
        const auto watcher = follow();
        QVERIFY(!watcher->fileEventsEnabled());
        QSignalSpy events(watcher.get(), &FileSystemWatcher::fileEvents);
        QSignalSpy spy(watcher.get(), &FileSystemWatcher::changed);

        touch(at(QStringLiteral("a/b/ka.wav")));
        QTRY_VERIFY_WITH_TIMEOUT(reported(collect(spy), at(QStringLiteral("a/b"))), 5000);
        QTest::qWait(300);
        QCOMPARE(events.size(), 0);
    }

    // Each kind of entry change is reported, and the directory is still reported by changed().
    void file_events_are_reported_if_enabled() {
        if (!FileSystemWatcher::fileEventsAvailable()) {
            QSKIP("File events are unavailable on this system.");
        }
        const auto watcher = follow(true);
        QVERIFY(watcher->fileEventsEnabled());
        QSignalSpy events(watcher.get(), &FileSystemWatcher::fileEvents);
        QSignalSpy spy(watcher.get(), &FileSystemWatcher::changed);

        const auto entries = [&] {
            QList<FileSystemWatcher::FileEvent> out;
            for (const auto &arguments : std::as_const(events)) {
                out += arguments.at(0).value<QList<FileSystemWatcher::FileEvent>>();
            }
            return out;
        };
        using Event = FileSystemWatcher::FileEvent;
        const QString ka = at(QStringLiteral("a/b/ka.wav"));
        const QString ki = at(QStringLiteral("a/b/ki.wav"));

        touch(ka);
        QTRY_VERIFY_WITH_TIMEOUT(entries().contains(Event{Event::Created, ka}), 5000);
        QVERIFY(QFile::rename(ka, ki));
        QTRY_VERIFY_WITH_TIMEOUT(entries().contains(Event{Event::Deleted, ka}), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(entries().contains(Event{Event::Created, ki}), 5000);
        QVERIFY(QFile::remove(ki));
        QTRY_VERIFY_WITH_TIMEOUT(entries().contains(Event{Event::Deleted, ki}), 5000);

        // The order of the reports follows the order of the changes.
        const auto all = entries();
        QVERIFY(all.indexOf(Event{Event::Created, ka}) < all.indexOf(Event{Event::Deleted, ka}));
        QVERIFY(all.indexOf(Event{Event::Created, ki}) < all.indexOf(Event{Event::Deleted, ki}));
        QVERIFY(reported(collect(spy), at(QStringLiteral("a/b"))));
    }

    // Where file events are unavailable, the request is ignored, so that the program is not
    // started with an option it rejects and monitoring continues.
    void file_events_are_ignored_where_unavailable() {
        if (FileSystemWatcher::fileEventsAvailable()) {
            QSKIP("File events are available on this system.");
        }
        const auto watcher = follow(true);
        QVERIFY(!watcher->fileEventsEnabled());
        QSignalSpy spy(watcher.get(), &FileSystemWatcher::changed);

        touch(at(QStringLiteral("a/b/ka.wav")));
        QTRY_VERIFY_WITH_TIMEOUT(reported(collect(spy), at(QStringLiteral("a/b"))), 5000);
    }

    // Without the program no changes are reported, and every root is reported as unwatchable,
    // so that the caller falls back to examining the disk itself.
    void without_the_program_every_root_is_unwatchable() {
        FileSystemWatcher watcher;
        watcher.setProgram(m_dir->path() + QStringLiteral("/no such program.exe"));
        QSignalSpy unwatchable(&watcher, &FileSystemWatcher::unwatchable);
        watcher.setRoots({root()});
        QTRY_COMPARE_WITH_TIMEOUT(unwatchable.size(), 1, 5000);
        QCOMPARE(unwatchable.at(0).at(0).toString(), root());
    }

#ifdef Q_OS_WIN
    // A program that does not send the expected greeting is not accepted as the monitor
    // program and is not restarted. Abandoning it only after several failures would require
    // restarts taking about one and a half seconds, which the time bound detects.
    void a_program_that_does_not_greet_is_not_used() {
        FileSystemWatcher watcher;
        watcher.setProgram(QStringLiteral("C:/Windows/System32/whoami.exe"));
        QSignalSpy unwatchable(&watcher, &FileSystemWatcher::unwatchable);
        watcher.setRoots({root()});
        QTRY_COMPARE_WITH_TIMEOUT(unwatchable.size(), 1, 1000);
        QTest::qWait(1000);
        QCOMPARE(unwatchable.size(), 1);
        QCOMPARE(watcher.processId(), 0);
    }
#endif
};

QTEST_GUILESS_MAIN(test_FileSystemWatcher)

#include "test_FileSystemWatcher.moc"
