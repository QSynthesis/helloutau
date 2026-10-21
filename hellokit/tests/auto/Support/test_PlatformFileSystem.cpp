#include <filesystem>
#include <fstream>

#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Support/PlatformFileSystem.h>

using namespace hello::kit;

namespace fs = std::filesystem;

namespace {

    // The name ga.wav with the kana ga, U+304C, in Normalization Form C
    QString composed() {
        return QString(QChar(0x304C)) + QStringLiteral(".wav");
    }

    // The same name in Normalization Form D, the kana ka, U+304B, and the combining voiced sound
    // mark, U+3099
    QString decomposed() {
        return QString(QChar(0x304B)) + QChar(0x3099) + QStringLiteral(".wav");
    }

    fs::path pathOf(const QString &text) {
        return fs::path(text.toStdU16String());
    }

}

class test_PlatformFileSystem : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // A decomposed name is composed on macOS and kept elsewhere. A name without combining marks
    // is kept everywhere.
    void a_listed_name_is_composed_on_macos() {
#ifdef Q_OS_MACOS
        QCOMPARE(PlatformFileSystem::listedFileName(pathOf(decomposed())), pathOf(composed()));
#else
        QCOMPARE(PlatformFileSystem::listedFileName(pathOf(decomposed())), pathOf(decomposed()));
#endif
        QCOMPARE(PlatformFileSystem::listedFileName(pathOf(composed())), pathOf(composed()));
        QCOMPARE(PlatformFileSystem::listedFileName(fs::path("a.wav")), fs::path("a.wav"));
    }

    // The listed name of a file written with a decomposed name finds the file.
    void the_listed_name_finds_the_file() {
        QTemporaryDir dir;
        const auto folder = pathOf(dir.path());
        std::ofstream(folder / pathOf(decomposed())) << "RIFF";
        std::error_code error;
        int count = 0;
        for (const auto &entry : fs::directory_iterator(folder, error)) {
            const auto listed = PlatformFileSystem::listedFileName(entry.path().filename());
            QVERIFY(fs::is_regular_file(folder / listed, error));
            ++count;
        }
        QVERIFY(!error);
        QCOMPARE(count, 1);
    }

    // A folder that can be listed, a missing folder and a file have no reason. A file is no
    // folder, and the callers treat it as missing.
    void a_readable_or_missing_folder_has_no_reason() {
        QTemporaryDir dir;
        const auto root = pathOf(dir.path());
        QCOMPARE(PlatformFileSystem::unreadableFolderReason(root), std::nullopt);
        QCOMPARE(PlatformFileSystem::unreadableFolderReason(root / "missing"), std::nullopt);
        QCOMPARE(PlatformFileSystem::unreadableFolderReason(root / "missing" / "deeper"),
                 std::nullopt);
    }

    // A folder without permissions has the reason of the system.
    void an_unreadable_folder_has_a_reason() {
#ifdef Q_OS_WINDOWS
        QSKIP("The permissions of std::filesystem do not deny reading on Windows.");
#else
        QTemporaryDir dir;
        const auto locked = pathOf(dir.path()) / "locked";
        fs::create_directories(locked);
        fs::permissions(locked, fs::perms::none);
        std::error_code error;
        fs::directory_iterator probe(locked, error);
        if (!error) {
            fs::permissions(locked, fs::perms::owner_all);
            QSKIP("The user reads a directory without permissions, as root does.");
        }
        const auto reason = PlatformFileSystem::unreadableFolderReason(locked);
        fs::permissions(locked, fs::perms::owner_all);
        QVERIFY(reason);
        QCOMPARE(*reason, PlatformFileSystem::accessErrorReason(error));
        QVERIFY(!reason->isEmpty());
#endif
    }

    // EPERM, which macOS returns for the Documents folder that it denies, names the setting
    // there, and the system message elsewhere.
    void a_denied_access_names_the_setting_on_macos() {
        const auto reason = PlatformFileSystem::accessErrorReason(
            std::make_error_code(std::errc::operation_not_permitted));
#ifdef Q_OS_MACOS
        QVERIFY(reason.contains(QStringLiteral("Files and Folders")));
#else
        QCOMPARE(reason, QString::fromLocal8Bit(
                             std::make_error_code(std::errc::operation_not_permitted).message()));
#endif
    }
};

QTEST_GUILESS_MAIN(test_PlatformFileSystem)

#include "test_PlatformFileSystem.moc"
