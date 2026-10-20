#include <filesystem>

#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Support/FolderAccess.h>

using namespace hello::kit;

namespace fs = std::filesystem;

class test_FolderAccess : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // A folder that can be listed, a missing folder and a file have no reason. A file is no
    // folder, and the callers treat it as missing.
    void a_readable_or_missing_folder_has_no_reason() {
        QTemporaryDir dir;
        const auto root = fs::path(dir.path().toStdU16String());
        QCOMPARE(FolderAccess::unreadableReason(root), std::nullopt);
        QCOMPARE(FolderAccess::unreadableReason(root / "missing"), std::nullopt);
        QCOMPARE(FolderAccess::unreadableReason(root / "missing" / "deeper"), std::nullopt);
    }

    // A folder without permissions has the reason of the system.
    void an_unreadable_folder_has_a_reason() {
#ifdef Q_OS_WINDOWS
        QSKIP("The permissions of std::filesystem do not deny reading on Windows.");
#else
        QTemporaryDir dir;
        const auto locked = fs::path(dir.path().toStdU16String()) / "locked";
        fs::create_directories(locked);
        fs::permissions(locked, fs::perms::none);
        std::error_code error;
        fs::directory_iterator probe(locked, error);
        if (!error) {
            fs::permissions(locked, fs::perms::owner_all);
            QSKIP("The user reads a directory without permissions, as root does.");
        }
        const auto reason = FolderAccess::unreadableReason(locked);
        fs::permissions(locked, fs::perms::owner_all);
        QVERIFY(reason);
        QCOMPARE(*reason, FolderAccess::reasonOf(error));
        QVERIFY(!reason->isEmpty());
#endif
    }

    // EPERM, which macOS returns for the Documents folder that it denies, names the setting
    // there, and the system message elsewhere.
    void a_denied_access_names_the_setting_on_macos() {
        const auto reason =
            FolderAccess::reasonOf(std::make_error_code(std::errc::operation_not_permitted));
#ifdef Q_OS_MACOS
        QVERIFY(reason.contains(QStringLiteral("Files and Folders")));
#else
        QCOMPARE(reason, QString::fromLocal8Bit(
                             std::make_error_code(std::errc::operation_not_permitted).message()));
#endif
    }
};

QTEST_GUILESS_MAIN(test_FolderAccess)

#include "test_FolderAccess.moc"
