#include <filesystem>
#include <thread>
#include <vector>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtCore/QThread>
#include <QtTest/QTest>

#ifndef Q_OS_WIN
#  include <sys/stat.h>
#  include <utime.h>
#endif

#include <hellokit/Support/FileIntegrity.h>

using namespace hello::kit;
namespace fs = std::filesystem;

namespace {

    fs::path pathOf(const QString &path) {
        return fs::path(path.toStdU16String());
    }

    bool writeFile(const QString &path, const QByteArray &content) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
               file.write(content) == content.size();
    }

    // The SHA-256 of "a"
    const QString sha256OfA =
        QStringLiteral("ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb");

    // Writes the same content again, which changes the identity, and sets the modification
    // time back, as a program that hides its write would.
    bool rewriteInPlace(const QString &path, const QByteArray &content) {
        QFile file(path);
        const auto modified = QFileInfo(path).lastModified();
        if (!file.open(QIODevice::ReadWrite) || file.write(content) != content.size()) {
            return false;
        }
        file.close();
        QThread::msleep(20);
        QFile again(path);
        return again.open(QIODevice::ReadWrite) &&
               again.setFileTime(modified, QFileDevice::FileModificationTime);
    }

}

class test_FileIntegrity : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString filePath(const QString &name = QStringLiteral("tool.exe")) const {
        return m_dir.filePath(name);
    }

private Q_SLOTS:
    void init() {
        QVERIFY(m_dir.isValid());
        QDir(m_dir.path()).removeRecursively();
        QVERIFY(QDir().mkpath(m_dir.path()));
    }

    // The fingerprint holds the SHA-256 of the content, and a file that cannot be read has
    // none, with the reason, rather than the SHA-256 of no content.
    void a_fingerprint_reads_the_content() {
        QVERIFY(writeFile(filePath(), "a"));
        QString reason;
        const auto fingerprint = FileFingerprint::of(pathOf(filePath()), &reason);
        QVERIFY2(fingerprint, qPrintable(reason));
        QCOMPARE(fingerprint->sha256(), sha256OfA);
        QCOMPARE(fingerprint->identity(), *FileFingerprint::Identity::of(pathOf(filePath())));

        QVERIFY(!FileFingerprint::of(pathOf(filePath(QStringLiteral("missing"))), &reason));
        QVERIFY(reason.contains(QStringLiteral("missing")));
        QVERIFY(!FileFingerprint::of(pathOf(m_dir.path())));
        QVERIFY(!FileFingerprint::Identity::of(pathOf(m_dir.path())));
    }

    // A rewrite, a write in place whose modification time is set back, and a replacement by a
    // rename each change the identity.
    void every_change_changes_the_identity() {
        QVERIFY(writeFile(filePath(), "a"));
        const auto path = pathOf(filePath());
        const auto first = FileFingerprint::Identity::of(path);
        QVERIFY(first);

        QVERIFY(rewriteInPlace(filePath(), "b"));
        const auto rewritten = FileFingerprint::Identity::of(path);
        QVERIFY(rewritten);
        QVERIFY(*rewritten != *first);

        QVERIFY(writeFile(filePath(QStringLiteral("other.exe")), "b"));
        QVERIFY(QFile::remove(filePath()));
        QVERIFY(QFile::rename(filePath(QStringLiteral("other.exe")), filePath()));
        const auto replaced = FileFingerprint::Identity::of(path);
        QVERIFY(replaced);
        QVERIFY(*replaced != *rewritten);
    }

    // The cache reads the content once while the identity is unchanged, and again after a
    // change. Without an instance, the content is read each time.
    void the_cache_reads_an_unchanged_file_once() {
        QVERIFY(writeFile(filePath(), "a"));
        const auto path = pathOf(filePath());
        QVERIFY(!FileFingerprintCache::instance());
        QCOMPARE(FileFingerprintCache::fingerprintOf(path)->sha256(), sha256OfA);
        {
            FileFingerprintCache cache;
            QCOMPARE(FileFingerprintCache::instance(), &cache);
            QCOMPARE(FileFingerprintCache::fingerprintOf(path)->sha256(), sha256OfA);
            QCOMPARE(FileFingerprintCache::fingerprintOf(path)->sha256(), sha256OfA);
            QCOMPARE(cache.readCount(), 1);

            QVERIFY(rewriteInPlace(filePath(), "b"));
            QVERIFY(FileFingerprintCache::fingerprintOf(path)->sha256() != sha256OfA);
            QCOMPARE(cache.readCount(), 2);

            // Several threads at once
            std::vector<std::thread> threads;
            std::atomic<int> agreed = 0;
            const auto expected = FileFingerprintCache::fingerprintOf(path)->sha256();
            for (int i = 0; i < 8; ++i) {
                threads.emplace_back([&] {
                    for (int j = 0; j < 50; ++j) {
                        if (FileFingerprintCache::fingerprintOf(path)->sha256() == expected) {
                            ++agreed;
                        }
                    }
                });
            }
            for (auto &thread : threads) {
                thread.join();
            }
            QCOMPARE(agreed.load(), 400);
            QCOMPARE(cache.readCount(), 2);
        }
        QVERIFY(!FileFingerprintCache::instance());
    }

    // A guard requires the expected content, and reads no content that the cache knows for
    // the identity.
    void a_guard_requires_the_expected_content() {
        QVERIFY(writeFile(filePath(), "a"));
        const auto path = pathOf(filePath());
        QString reason;
        QVERIFY(!FileGuard::hold(
            {
                {path, QStringLiteral("00")}
        },
            &reason));
        QVERIFY(reason.contains(QStringLiteral("tool.exe")));
        QVERIFY(!FileGuard::hold({
            {pathOf(filePath(QStringLiteral("missing"))), sha256OfA}
        }));

        FileFingerprintCache cache;
        QVERIFY(FileFingerprintCache::fingerprintOf(path));
        QCOMPARE(cache.readCount(), 1);
        const auto guard = FileGuard::hold(
            {
                {path, sha256OfA.toUpper()}
        },
            &reason);
        QVERIFY2(guard, qPrintable(reason));
        QCOMPARE(cache.readCount(), 1);
        QVERIFY(guard->isIntact());
        QVERIFY(!guard->changedFile());
        QVERIFY(guard->changeMessage().isEmpty());
    }

#ifdef Q_OS_WIN
    // On Windows, a held file cannot be written, renamed or deleted.
    void a_held_file_cannot_be_replaced_on_windows() {
        QVERIFY(writeFile(filePath(), "a"));
        const auto path = pathOf(filePath());
        {
            const auto guard = FileGuard::hold({
                { path,
                 sha256OfA }
            });
            QVERIFY(guard);
            QFile file(filePath());
            QVERIFY(!file.open(QIODevice::WriteOnly | QIODevice::Append));
            QVERIFY(!QFile::rename(filePath(), filePath(QStringLiteral("moved.exe"))));
            QVERIFY(!QFile::remove(filePath()));
            QVERIFY(guard->isIntact());
            // Its fingerprint can still be read.
            QCOMPARE(FileFingerprint::of(path)->sha256(), sha256OfA);
        }
        QVERIFY(QFile::remove(filePath()));
    }
#else
    // Without a mandatory lock, a change is detected once it has happened, and the guard stays
    // broken.
    void a_changed_file_breaks_the_guard() {
        QVERIFY(writeFile(filePath(), "a"));
        QVERIFY(writeFile(filePath(QStringLiteral("second.exe")), "a"));
        const auto guard = FileGuard::hold({
            {pathOf(filePath()),                             sha256OfA},
            {pathOf(filePath(QStringLiteral("second.exe"))), sha256OfA}
        });
        QVERIFY(guard);
        QVERIFY(guard->isIntact());
        QVERIFY(rewriteInPlace(filePath(QStringLiteral("second.exe")), "a"));
        QVERIFY(!guard->isIntact());
        QCOMPARE(guard->changedFile(),
                 std::optional(pathOf(filePath(QStringLiteral("second.exe")))));
        QVERIFY(guard->changeMessage().contains(QStringLiteral("second.exe")));
        QVERIFY(!guard->isIntact());
    }

    // The permissions of the file and of its folder count, the sticky bit of the folder
    // excepted.
    void the_permissions_of_others_are_reported() {
        QVERIFY(writeFile(filePath(), "a"));
        const auto path = pathOf(filePath());
        const auto file = QFile::encodeName(filePath());
        const auto folder = QFile::encodeName(m_dir.path());
        QVERIFY(::chmod(folder.constData(), 0700) == 0);
        QVERIFY(::chmod(file.constData(), 0700) == 0);
        QVERIFY(!FilePermissions::writableByOthers(path));

        QVERIFY(::chmod(file.constData(), 0702) == 0);
        QVERIFY(FilePermissions::writableByOthers(path));
        QVERIFY(::chmod(file.constData(), 0700) == 0);

        QVERIFY(::chmod(folder.constData(), 0703) == 0);
        const auto reason = FilePermissions::writableByOthers(path);
        QVERIFY(reason);
        QVERIFY(reason->contains(m_dir.path()));
        QVERIFY(::chmod(folder.constData(), 01703) == 0);
        QVERIFY(!FilePermissions::writableByOthers(path));
        QVERIFY(::chmod(folder.constData(), 0700) == 0);
    }
#endif
};

QTEST_GUILESS_MAIN(test_FileIntegrity)

#include "test_FileIntegrity.moc"
