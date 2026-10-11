#include <algorithm>
#include <filesystem>
#include <fstream>
#include <system_error>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Support/TemporaryStorage.h>

using namespace hello::kit;

namespace fs = std::filesystem;

namespace {

    fs::path pathOf(const QString &text) {
        return fs::path(text.toStdU16String());
    }

    fs::path testRoot() {
        return pathOf(QDir::cleanPath(QDir::tempPath() + QStringLiteral("/hellokit-tests/") +
                                      QCoreApplication::applicationName()));
    }

}

class test_TemporaryStorage : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The storage has a session directory in its root, in which the templates create the
    // temporary directories. Its destruction removes the session directory with its content,
    // and the templates then lie in the temporary directory of the system.
    void the_session_lasts_as_long_as_the_storage() {
        QVERIFY(!TemporaryStorage::instance());
        fs::path session;
        {
            const TemporaryStorage storage(testRoot());
            QCOMPARE(TemporaryStorage::instance(), &storage);
            QCOMPARE(storage.root(), testRoot());
            QVERIFY(storage.sessionDirectory());
            session = *storage.sessionDirectory();
            QCOMPARE(session.parent_path(), testRoot());
            QCOMPARE(TemporaryStorage::location(), session);

            QTemporaryDir directory(TemporaryStorage::templatePath(QStringLiteral("render")));
            QVERIFY(directory.isValid());
            const auto path = pathOf(directory.path());
            QCOMPARE(path.parent_path(), session);
            QVERIFY(QString::fromStdU16String(path.filename().u16string())
                        .startsWith(QStringLiteral("render-")));
            directory.setAutoRemove(false);
            std::ofstream(path / "left.wav") << "RIFF";
        }
        QVERIFY(!fs::exists(session));
        QVERIFY(!TemporaryStorage::instance());
        QCOMPARE(TemporaryStorage::location(), pathOf(QDir::tempPath()));
    }

    // A session directory without a running process is reported as exited, and the session of
    // the storage is not.
    void the_sessions_of_exited_processes_are_reported() {
        const TemporaryStorage storage(testRoot());
        QVERIFY(storage.sessionDirectory());
        const auto exited = testRoot() / "exited-session";
        std::error_code error;
        fs::create_directories(exited, error);
        const auto sessions = storage.exitedSessions();
        fs::remove_all(exited, error);

        QVERIFY(std::find(sessions.begin(), sessions.end(), exited) != sessions.end());
        QVERIFY(std::find(sessions.begin(), sessions.end(), *storage.sessionDirectory()) ==
                sessions.end());
        QVERIFY(fs::is_directory(*storage.sessionDirectory()));
    }
};

QTEST_GUILESS_MAIN(test_TemporaryStorage)

#include "test_TemporaryStorage.moc"
