#include <filesystem>
#include <string>
#include <vector>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <zip.h>

#include <stdcorelib/support/popen.h>

#include <helloutau/Editor/SoftwareTreeUpdate.h>

using namespace hello::daw;

namespace fs = std::filesystem;

namespace {

    std::string utf8(const QString &text) {
        const auto bytes = text.toUtf8();
        return std::string(bytes.constData(), size_t(bytes.size()));
    }

    // Runs the program arguments and returns whether it exits with 0.
    bool run(std::vector<std::string> arguments) {
        stdc::Popen process;
        process.args(std::move(arguments));
        process.standardOutput(stdc::Popen::DeviceNull);
        process.standardError(stdc::Popen::DeviceNull);
        if (!process.start() || !process.wait()) {
            return false;
        }
        const auto code = process.returnCode();
        return code && *code == 0;
    }

    // Writes an archive with the files in entries, each a name and a content.
    bool writeArchive(const QString &path,
                      const QList<std::pair<QByteArray, QByteArray>> &entries) {
        int error = 0;
        const auto zip =
            zip_open(QFile::encodeName(path).constData(), ZIP_CREATE | ZIP_TRUNCATE, &error);
        if (!zip) {
            return false;
        }
        for (const auto &[name, data] : entries) {
            const auto source =
                zip_source_buffer(zip, data.constData(), zip_uint64_t(data.size()), 0);
            if (!source || zip_file_add(zip, name.constData(), source, ZIP_FL_ENC_UTF_8) < 0) {
                zip_source_free(source);
                zip_discard(zip);
                return false;
            }
        }
        return zip_close(zip) == 0;
    }

    bool writeFile(const QString &path, const QByteArray &data) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
    }

    QByteArray contentOf(const QString &path) {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

}

class test_SoftwareTreeUpdate : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_root;

    QString installDirectory() const {
        return m_root.filePath(QStringLiteral("HelloUtau"));
    }

    QString archivePath() const {
        return m_root.filePath(QStringLiteral("update.zip"));
    }

    // The archive as the release packages it: one top directory, with the program in bin
    bool writeRelease() const {
        return writeArchive(archivePath(), {
                                               {"helloutau-2/bin/program",    "new program"},
                                               {"helloutau-2/share/data.txt", "new data"   }
        });
    }

    QString imagePath() const {
        return m_root.filePath(QStringLiteral("update.dmg"));
    }

    // Writes the disk image as the release packages it, with a link to the Applications folder
    // and, if withBundle is true, a bundle with its program and a framework whose Current is a
    // symbolic link. Only on macOS.
    bool writeImage(bool withBundle) const {
        const auto folder = m_root.filePath(QStringLiteral("image"));
        QDir(folder).removeRecursively();
        if (withBundle) {
            const auto bundle = folder + QStringLiteral("/HelloUtau.app/Contents");
            const auto versions = bundle + QStringLiteral("/Frameworks/Q.framework/Versions");
            if (!writeFile(bundle + QStringLiteral("/MacOS/program"), "new program") ||
                !writeFile(versions + QStringLiteral("/A/Q"), "framework")) {
                return false;
            }
            fs::create_directory_symlink("A", fs::path(versions.toStdU16String()) / "Current");
        } else if (!writeFile(folder + QStringLiteral("/readme.txt"), "no bundle")) {
            return false;
        }
        fs::create_directory_symlink("/Applications",
                                     fs::path(folder.toStdU16String()) / "Applications");
        QFile::remove(imagePath());
        return run({"/usr/bin/hdiutil", "create", "-volname", "Update", "-srcfolder", utf8(folder),
                    "-format", "UDZO", "-quiet", utf8(imagePath())});
    }

private Q_SLOTS:
    void init() {
        QVERIFY(m_root.isValid());
        for (const auto &name : {"HelloUtau", "HelloUtau.new", "HelloUtau.old"}) {
            QDir(m_root.filePath(QLatin1String(name))).removeRecursively();
        }
        QVERIFY(writeFile(installDirectory() + QStringLiteral("/bin/program"), "old program"));
    }

    void the_new_version_is_extracted_beside_the_installation() {
        QVERIFY(writeRelease());
        SoftwareTreeUpdate update(installDirectory());
        QCOMPARE(update.newDirectory(), installDirectory() + QStringLiteral(".new"));
        QCOMPARE(update.oldDirectory(), installDirectory() + QStringLiteral(".old"));
        QSignalSpy finished(&update, &SoftwareTreeUpdate::finished);
        QSignalSpy progress(&update, &SoftwareTreeUpdate::progressChanged);
        update.startExtraction(archivePath(), QStringLiteral("bin/program"), 2);
        QVERIFY(update.isRunning());
        QVERIFY(finished.wait());
        QVERIFY(!update.isRunning());
        QVERIFY2(update.errorString().isEmpty(), qPrintable(update.errorString()));
        QCOMPARE(contentOf(update.newDirectory() + QStringLiteral("/bin/program")),
                 QByteArray("new program"));
        QCOMPARE(contentOf(update.newDirectory() + QStringLiteral("/share/data.txt")),
                 QByteArray("new data"));
        QCOMPARE(contentOf(installDirectory() + QStringLiteral("/bin/program")),
                 QByteArray("old program"));
        QVERIFY(!progress.isEmpty());
    }

    void an_existing_new_directory_is_replaced() {
        QVERIFY(writeRelease());
        SoftwareTreeUpdate update(installDirectory());
        QVERIFY(writeFile(update.newDirectory() + QStringLiteral("/stale.txt"), "stale"));
        QSignalSpy finished(&update, &SoftwareTreeUpdate::finished);
        update.startExtraction(archivePath(), QStringLiteral("bin/program"), 1);
        QVERIFY(finished.wait());
        QVERIFY(update.errorString().isEmpty());
        QVERIFY(!QFileInfo::exists(update.newDirectory() + QStringLiteral("/stale.txt")));
    }

    void a_missing_program_fails_and_removes_the_new_directory() {
        QVERIFY(writeRelease());
        SoftwareTreeUpdate update(installDirectory());
        QSignalSpy finished(&update, &SoftwareTreeUpdate::finished);
        update.startExtraction(archivePath(), QStringLiteral("bin/other"), 1);
        QVERIFY(finished.wait());
        QVERIFY(!update.errorString().isEmpty());
        QVERIFY(!QFileInfo::exists(update.newDirectory()));
    }

    void an_unsafe_archive_fails_and_removes_the_new_directory() {
        QVERIFY(writeArchive(archivePath(), {
                                                {"helloutau-2/bin/program", "new program"},
                                                {"../evil.txt",             "evil"       }
        }));
        SoftwareTreeUpdate update(installDirectory());
        QVERIFY(writeFile(update.newDirectory() + QStringLiteral("/stale.txt"), "stale"));
        QSignalSpy finished(&update, &SoftwareTreeUpdate::finished);
        update.startExtraction(archivePath(), QStringLiteral("bin/program"), 1);
        QVERIFY(finished.wait());
        QVERIFY(!update.errorString().isEmpty());
        QVERIFY(!QFileInfo::exists(update.newDirectory()));
        QVERIFY(!QFileInfo::exists(m_root.filePath(QStringLiteral("evil.txt"))));
    }

    void cancel_removes_the_new_directory_without_finished() {
        QVERIFY(writeRelease());
        SoftwareTreeUpdate update(installDirectory());
        QSignalSpy finished(&update, &SoftwareTreeUpdate::finished);
        update.startExtraction(archivePath(), QStringLiteral("bin/program"), 1);
        update.cancel();
        QTRY_VERIFY(!update.isRunning());
        // Events that the extraction posted are processed, and none emits finished().
        QCoreApplication::processEvents();
        QCOMPARE(finished.size(), 0);
        QVERIFY(!QFileInfo::exists(update.newDirectory()));
    }

    void a_destroyed_update_stops_the_extraction() {
        QVERIFY(writeRelease());
        QString newDirectory;
        {
            SoftwareTreeUpdate update(installDirectory());
            newDirectory = update.newDirectory();
            update.startExtraction(archivePath(), QStringLiteral("bin/program"), 1);
        }
        QCoreApplication::processEvents();
        QVERIFY(!QFileInfo::exists(newDirectory));
    }

    void a_restarted_extraction_reports_once() {
        QVERIFY(writeRelease());
        SoftwareTreeUpdate update(installDirectory());
        QSignalSpy finished(&update, &SoftwareTreeUpdate::finished);
        update.startExtraction(archivePath(), QStringLiteral("bin/other"), 1);
        update.startExtraction(archivePath(), QStringLiteral("bin/program"), 1);
        QVERIFY(finished.wait());
        QCoreApplication::processEvents();
        QCOMPARE(finished.size(), 1);
        QVERIFY(update.errorString().isEmpty());
    }

    // The bundle keeps the symbolic links of its frameworks.
    void a_bundle_is_copied_from_a_disk_image() {
#ifndef Q_OS_MACOS
        QSKIP("Disk images are opened only on macOS.");
#else
        QVERIFY(writeImage(true));
        SoftwareTreeUpdate update(installDirectory());
        QSignalSpy finished(&update, &SoftwareTreeUpdate::finished);
        update.startImageCopy(imagePath(), QStringLiteral("Contents/MacOS/program"));
        QVERIFY(update.isRunning());
        QVERIFY(finished.wait(60000));
        QVERIFY2(update.errorString().isEmpty(), qPrintable(update.errorString()));
        QCOMPARE(contentOf(update.newDirectory() + QStringLiteral("/Contents/MacOS/program")),
                 QByteArray("new program"));
        const auto current = fs::path(update.newDirectory().toStdU16String()) /
                             "Contents/Frameworks/Q.framework/Versions/Current";
        QVERIFY(fs::is_symlink(current));
        QCOMPARE(fs::read_symlink(current), fs::path("A"));
#endif
    }

    // On other systems than macOS, every copy from a disk image fails.
    void an_image_without_a_bundle_fails_and_removes_the_new_directory() {
#ifdef Q_OS_MACOS
        QVERIFY(writeImage(false));
#endif
        SoftwareTreeUpdate update(installDirectory());
        QVERIFY(writeFile(update.newDirectory() + QStringLiteral("/stale.txt"), "stale"));
        QSignalSpy finished(&update, &SoftwareTreeUpdate::finished);
        update.startImageCopy(imagePath(), QStringLiteral("Contents/MacOS/program"));
        QVERIFY(finished.wait(60000));
        QVERIFY(!update.errorString().isEmpty());
        QVERIFY(!QFileInfo::exists(update.newDirectory()));
    }

    void a_bundle_on_a_disk_image_is_isolated() {
        QVERIFY(!SoftwareTreeUpdate::isIsolated(installDirectory()));
#ifdef Q_OS_MACOS
        QVERIFY(SoftwareTreeUpdate::isIsolated(
            QStringLiteral("/private/var/folders/a/b/T/AppTranslocation/C/d/HelloUtau.app")));
        QVERIFY(writeImage(true));
        const auto mount = m_root.filePath(QStringLiteral("mount"));
        QVERIFY(QDir().mkpath(mount));
        QVERIFY(run({"/usr/bin/hdiutil", "attach", utf8(imagePath()), "-nobrowse", "-noautoopen",
                     "-readonly", "-mountpoint", utf8(mount), "-quiet"}));
        const bool isolated =
            SoftwareTreeUpdate::isIsolated(mount + QStringLiteral("/HelloUtau.app"));
        QVERIFY(run({"/usr/bin/hdiutil", "detach", utf8(mount), "-quiet"}));
        QVERIFY(isolated);
#endif
    }

    void the_directory_beside_the_installation_is_writable() {
        QVERIFY(SoftwareTreeUpdate(installDirectory()).canReplace());
        QVERIFY(
            !SoftwareTreeUpdate(m_root.filePath(QStringLiteral("missing/HelloUtau"))).canReplace());
    }

    void the_updater_arguments_name_the_directories() {
        SoftwareTreeUpdate update(installDirectory());
        const auto log = m_root.filePath(QStringLiteral("updater.log"));
        const QStringList common{
            QStringLiteral("--pid"),         QStringLiteral("42"),
            QStringLiteral("--target"),      QDir::toNativeSeparators(installDirectory()),
            QStringLiteral("--source"),      QDir::toNativeSeparators(update.newDirectory()),
            QStringLiteral("--log"),         QDir::toNativeSeparators(log),
            QStringLiteral("--show-errors"),
        };
        QCOMPARE(update.updaterArguments(42, log, QString()), common);
        const auto program = installDirectory() + QStringLiteral("/bin/program");
        QCOMPARE(
            update.updaterArguments(42, log, program),
            (common + QStringList{QStringLiteral("--launch"), QDir::toNativeSeparators(program)}));
    }

    // The updater of the application is in the directory of the program. The copy of the updater
    // of the build replaces an existing copy.
    void the_updater_is_copied_into_the_directory() {
        QCOMPARE(SoftwareTreeUpdate::defaultUpdater(), QCoreApplication::applicationDirPath() +
                                                           QLatin1Char('/') +
                                                           SoftwareTreeUpdate::updaterFileName());
        const auto updater = QStringLiteral(TEST_PROGRAM_HELLO_UPDATER);
        const auto directory = m_root.filePath(QStringLiteral("updates"));
        const auto copy = SoftwareTreeUpdate::copyUpdater(updater, directory);
        QCOMPARE(copy, directory + QLatin1Char('/') + SoftwareTreeUpdate::updaterFileName());
        QCOMPARE(QFileInfo(copy).size(), QFileInfo(updater).size());
        QCOMPARE(SoftwareTreeUpdate::copyUpdater(updater, directory), copy);
    }

    void the_remnants_are_removed() {
        SoftwareTreeUpdate update(installDirectory());
        QVERIFY(writeFile(update.newDirectory() + QStringLiteral("/bin/program"), "new"));
        QVERIFY(writeFile(update.oldDirectory() + QStringLiteral("/bin/program"), "old"));
        update.removeRemnants();
        QVERIFY(!QFileInfo::exists(update.newDirectory()));
        QVERIFY(!QFileInfo::exists(update.oldDirectory()));
        QCOMPARE(contentOf(installDirectory() + QStringLiteral("/bin/program")),
                 QByteArray("old program"));
    }

    void the_log_is_read() {
        const auto log = m_root.filePath(QStringLiteral("updater.log"));
        QFile::remove(log);
        QVERIFY(SoftwareTreeUpdate::readLog(log).isEmpty());
        QVERIFY(writeFile(log, "hello-updater: cannot rename a to b\n"));
        QCOMPARE(SoftwareTreeUpdate::readLog(log),
                 QStringLiteral("hello-updater: cannot rename a to b"));
    }
};

QTEST_GUILESS_MAIN(test_SoftwareTreeUpdate)

#include "test_SoftwareTreeUpdate.moc"
