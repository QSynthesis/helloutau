#include <filesystem>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <zip.h>

#include <hellokit/Support/ExitLauncher.h>
#include <hellokit/Support/TemporaryStorage.h>

#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Editor/UpdateChecker.h>
#include <helloutau/Editor/UpdateTask.h>

using namespace hello;
using namespace hello::daw;

namespace {

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

    kit::SoftwareRelease releaseOf(const QString &version) {
        kit::SoftwareRelease release;
        release.tag = QLatin1Char('v') + version;
        release.version = QVersionNumber::fromString(version);
        release.page = QUrl(QStringLiteral("https://example.com/release"));
        return release;
    }

}

class test_UpdateTask : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_root;
    std::unique_ptr<kit::TemporaryStorage> m_storage;
    std::unique_ptr<kit::ExitLauncher> m_launcher;
    std::unique_ptr<AppSettings> m_settings;

    QString installDirectory() const {
        return m_root.filePath(QStringLiteral("HelloUtau"));
    }

    // The archive of a release, in the download directory as a download leaves it, with the
    // program of this test in bin
    QString writeRelease(const QString &name = QStringLiteral("app.zip")) const {
        const auto program = QByteArray("app-2/bin/") +
                             QFileInfo(QCoreApplication::applicationFilePath()).fileName().toUtf8();
        QDir().mkpath(UpdateChecker::downloadDirectory());
        const auto path = UpdateChecker::downloadDirectory() + QLatin1Char('/') + name;
        return writeArchive(path,
                            {
                                {program,                "new" },
                                {"app-2/share/data.txt", "data"}
        })
                   ? path
                   : QString();
    }

private Q_SLOTS:
    void init() {
        QVERIFY(m_root.isValid());
        m_storage = std::make_unique<kit::TemporaryStorage>(
            std::filesystem::path(m_root.filePath(QStringLiteral("temp")).toStdU16String()));
        m_launcher = std::make_unique<kit::ExitLauncher>();
        m_settings =
            std::make_unique<AppSettings>(m_root.filePath(QStringLiteral("settings.json")));
        QDir(installDirectory() + QStringLiteral(".new")).removeRecursively();
        QDir().mkpath(installDirectory() + QStringLiteral("/bin"));
    }

    void cleanup() {
        m_settings.reset();
        m_launcher.reset();
        m_storage.reset();
    }

    void an_offer_makes_the_release_available() {
        UpdateTask task(*m_settings, UpdateTask::Portable, installDirectory());
        QCOMPARE(task.state(), UpdateTask::Idle);
        QSignalSpy states(&task, &UpdateTask::stateChanged);
        task.offer(releaseOf(QStringLiteral("2.0")), QStringLiteral("app.zip"));
        QCOMPARE(task.state(), UpdateTask::Available);
        QCOMPARE(task.release()->version, QVersionNumber(2, 0));
        QCOMPARE(task.assetName(), QStringLiteral("app.zip"));
        QCOMPARE(states.size(), 1);
        // Nothing is prepared, so that nothing can be scheduled.
        QVERIFY(!task.schedule(false));
    }

    void the_portable_application_is_extracted_and_scheduled() {
        UpdateTask task(*m_settings, UpdateTask::Portable, installDirectory());
        task.offer(releaseOf(QStringLiteral("2.0")), QStringLiteral("app.zip"));
        const auto archive = writeRelease();
        QVERIFY(!archive.isEmpty());
        task.prepareFrom(archive);
        QCOMPARE(task.state(), UpdateTask::Extracting);
        QVERIFY(task.isRunning());
        QTRY_COMPARE(task.state(), UpdateTask::Ready);
        QVERIFY(QFileInfo::exists(installDirectory() + QStringLiteral(".new/share/data.txt")));

        m_launcher->scheduleRestart();
        QVERIFY(task.schedule(true));
        QVERIFY(task.isScheduled());
        QVERIFY(!m_launcher->isRestartScheduled());
        QVERIFY(QFileInfo::exists(UpdateChecker::downloadDirectory() + QLatin1Char('/') +
                                  QStringLiteral("hello-updater") +
                                  (QSysInfo::productType() == QLatin1String("windows")
                                       ? QStringLiteral(".exe")
                                       : QString())));
        task.unschedule();
        QVERIFY(!task.isScheduled());
    }

    // Canceling a prepared update removes the download and the extracted directory.
    void cancel_discards_the_prepared_update() {
        UpdateTask task(*m_settings, UpdateTask::Portable, installDirectory());
        task.offer(releaseOf(QStringLiteral("2.0")), QStringLiteral("app.zip"));
        const auto archive = writeRelease();
        task.prepareFrom(archive);
        QTRY_COMPARE(task.state(), UpdateTask::Ready);
        QVERIFY(task.schedule(false));
        task.cancel();
        QCOMPARE(task.state(), UpdateTask::Available);
        QVERIFY(!task.isScheduled());
        QVERIFY(!QFileInfo::exists(archive));
        QVERIFY(!QFileInfo::exists(installDirectory() + QStringLiteral(".new")));
    }

    void cancel_stops_the_extraction() {
        UpdateTask task(*m_settings, UpdateTask::Portable, installDirectory());
        task.offer(releaseOf(QStringLiteral("2.0")), QStringLiteral("app.zip"));
        const auto archive = writeRelease();
        task.prepareFrom(archive);
        task.cancel();
        QCOMPARE(task.state(), UpdateTask::Available);
        QVERIFY(!task.isRunning());
        QVERIFY(!QFileInfo::exists(archive));
        QTRY_VERIFY(!QFileInfo::exists(installDirectory() + QStringLiteral(".new")));
        QCoreApplication::processEvents();
        QCOMPARE(task.state(), UpdateTask::Available);
    }

    void an_archive_without_the_program_fails() {
        UpdateTask task(*m_settings, UpdateTask::Portable, installDirectory());
        task.offer(releaseOf(QStringLiteral("2.0")), QStringLiteral("other.zip"));
        const auto archive = UpdateChecker::downloadDirectory() + QStringLiteral("/other.zip");
        QDir().mkpath(UpdateChecker::downloadDirectory());
        QVERIFY(writeArchive(archive, {
                                          {"app-2/readme.txt", "readme"}
        }));
        task.prepareFrom(archive);
        QTRY_COMPARE(task.state(), UpdateTask::Failed);
        QVERIFY(!task.errorString().isEmpty());
        QVERIFY(!QFileInfo::exists(archive));
    }

    // The installer of the installed application needs no extraction.
    void the_installer_is_scheduled_at_once() {
        UpdateTask task(*m_settings, UpdateTask::Installed, installDirectory());
        task.offer(releaseOf(QStringLiteral("2.0")), QStringLiteral("app-setup.exe"));
        const auto installer =
            UpdateChecker::downloadDirectory() + QStringLiteral("/app-setup.exe");
        QDir().mkpath(UpdateChecker::downloadDirectory());
        QFile file(installer);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        task.prepareFrom(installer);
        QCOMPARE(task.state(), UpdateTask::Ready);
        m_launcher->scheduleRestart();
        QVERIFY(task.schedule(false));
        QVERIFY(task.isScheduled());
        // Without the relaunch, the scheduled restart remains.
        QVERIFY(m_launcher->isRestartScheduled());
    }
};

QTEST_GUILESS_MAIN(test_UpdateTask)

#include "test_UpdateTask.moc"
