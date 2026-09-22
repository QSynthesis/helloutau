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
#endif

using namespace hello::kit;

namespace {

    /// Everything changed() named since the spy was made, one list for each kind.
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

}

class test_FileSystemWatcher : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

    // A percent sign in the name, since that is what the protocol escapes, and one followed by
    // what reads as an escape, which is what goes wrong if it is not escaped.
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

    /// A watcher following root() , and ready.
    std::unique_ptr<FileSystemWatcher> follow() {
        auto watcher = std::make_unique<FileSystemWatcher>();
        watcher->setDelay(50);
        QSignalSpy ready(watcher.get(), &FileSystemWatcher::ready);
        watcher->setRoots({root()});
        [&] { QVERIFY(ready.wait(10000)); }();
        return watcher;
    }

private Q_SLOTS:
    void init() {
#ifndef Q_OS_WIN
        QSKIP("The helper follows nothing yet on this system.");
#endif
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
        QTRY_VERIFY_WITH_TIMEOUT(collect(spy).directories.contains(at(QStringLiteral("a/b"))),
                                 5000);
    }

    // The reason this is not QFileSystemWatcher: that one holds every directory it follows
    // open on Windows, and a bank's author could not rename or remove any of them.
    void what_is_followed_can_be_renamed_and_removed() {
        QVERIFY(QDir().mkpath(at(QStringLiteral("gone"))));
        QVERIFY(QDir().mkpath(at(QStringLiteral("old"))));
        const auto watcher = follow();
        QSignalSpy gone(watcher.get(), &FileSystemWatcher::rootGone);

        QVERIFY(QDir(at(QStringLiteral("gone"))).removeRecursively());
        QVERIFY(QDir().rename(at(QStringLiteral("old")), at(QStringLiteral("new"))));
        QVERIFY(QDir().rename(root(), root() + QStringLiteral(" renamed")));
        QTRY_COMPARE_WITH_TIMEOUT(gone.size(), 1, 5000);
        QCOMPARE(gone.at(0).at(0).toString(), root());

        // And the directory holding the root, back where it was.
        QVERIFY(QDir().rename(root() + QStringLiteral(" renamed"), root()));
        QVERIFY(QDir().rename(m_dir->path() + QStringLiteral("/voice"),
                              m_dir->path() + QStringLiteral("/voice2")));
    }

    // A directory that arrives whole has everything in it to look at, not only its name.
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
        QTRY_VERIFY_WITH_TIMEOUT(collect(spy).directories.contains(at(QStringLiteral("a"))), 5000);

        for (const auto &directory : collect(spy).directories) {
            QVERIFY2(directory.startsWith(root()), qPrintable(directory));
        }
    }

    void a_root_that_is_not_there_is_gone() {
        FileSystemWatcher watcher;
        QSignalSpy gone(&watcher, &FileSystemWatcher::rootGone);
        const QString missing = m_dir->path() + QStringLiteral("/nothing here");
        watcher.setRoots({missing});
        QTRY_COMPARE_WITH_TIMEOUT(gone.size(), 1, 10000);
        QCOMPARE(gone.at(0).at(0).toString(), missing);
    }

    // What happened while it was gone is not known, so every root is named once it is back.
    void a_helper_that_dies_is_started_again_and_every_root_named() {
        const auto watcher = follow();
        QSignalSpy spy(watcher.get(), &FileSystemWatcher::changed);
        QSignalSpy ready(watcher.get(), &FileSystemWatcher::ready);

        const qint64 first = watcher->helperProcessId();
        QVERIFY(first != 0);
#ifdef Q_OS_WIN
        const HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, DWORD(first));
        QVERIFY(process);
        QVERIFY(TerminateProcess(process, 1));
        CloseHandle(process);
#endif

        QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(collect(spy).trees.contains(root()), 5000);
        QVERIFY(watcher->helperProcessId() != 0);
        QVERIFY(watcher->helperProcessId() != first);

        // And it follows again.
        touch(at(QStringLiteral("a/after.wav")));
        QTRY_VERIFY_WITH_TIMEOUT(collect(spy).directories.contains(at(QStringLiteral("a"))), 5000);
    }

    // Without a helper nothing is reported, and every root is said to be unwatchable, so that
    // the caller knows to look at the disk itself.
    void without_the_helper_every_root_is_unwatchable() {
        FileSystemWatcher watcher;
        watcher.setHelper(m_dir->path() + QStringLiteral("/no such program.exe"));
        QSignalSpy unwatchable(&watcher, &FileSystemWatcher::unwatchable);
        watcher.setRoots({root()});
        QTRY_COMPARE_WITH_TIMEOUT(unwatchable.size(), 1, 5000);
        QCOMPARE(unwatchable.at(0).at(0).toString(), root());
    }

#ifdef Q_OS_WIN
    // A program that does not greet as the helper does is not trusted to be one, and not
    // started again either. Giving up only after it died a few times would take restarts, a
    // second and a half of them, which is what the bound tells apart.
    void a_program_that_is_not_the_helper_is_not_used() {
        FileSystemWatcher watcher;
        watcher.setHelper(QStringLiteral("C:/Windows/System32/whoami.exe"));
        QSignalSpy unwatchable(&watcher, &FileSystemWatcher::unwatchable);
        watcher.setRoots({root()});
        QTRY_COMPARE_WITH_TIMEOUT(unwatchable.size(), 1, 1000);
        QTest::qWait(1000);
        QCOMPARE(unwatchable.size(), 1);
        QCOMPARE(watcher.helperProcessId(), 0);
    }
#endif
};

QTEST_GUILESS_MAIN(test_FileSystemWatcher)

#include "test_FileSystemWatcher.moc"
