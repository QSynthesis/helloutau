#include <filesystem>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

#include <hellokit/Support/ExitLauncher.h>
#include <hellokit/Support/TemporaryStorage.h>

#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Editor/UpdateChecker.h>
#include <helloutau/Editor/UpdateTask.h>
#include <helloutau/Editor/UpdateTaskView.h>

using namespace hello;
using namespace hello::daw;

namespace {

    kit::SoftwareRelease releaseOf(const QString &version) {
        kit::SoftwareRelease release;
        release.tag = QLatin1Char('v') + version;
        release.version = QVersionNumber::fromString(version);
        release.notes = QStringLiteral("Release notes");
        release.page = QUrl(QStringLiteral("https://example.com/release"));
        return release;
    }

}

class test_UpdateTaskView : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_root;
    std::unique_ptr<kit::TemporaryStorage> m_storage;
    std::unique_ptr<kit::ExitLauncher> m_launcher;
    std::unique_ptr<AppSettings> m_settings;

    QPushButton *button(const UpdateTaskView &view, const char *name) const {
        return view.findChild<QPushButton *>(QLatin1String(name));
    }

    // Whether the button named \a name is shown in \a view
    bool shows(const UpdateTaskView &view, const char *name) const {
        return button(view, name)->isVisibleTo(&view);
    }

    // Prepares the installation of an installer, which needs no extraction
    void makeReady(UpdateTask &task) const {
        task.offer(releaseOf(QStringLiteral("2.0")), QStringLiteral("app-setup.exe"));
        const auto installer =
            UpdateChecker::downloadDirectory() + QStringLiteral("/app-setup.exe");
        QDir().mkpath(UpdateChecker::downloadDirectory());
        QFile file(installer);
        if (file.open(QIODevice::WriteOnly)) {
            file.close();
        }
        task.prepareFrom(installer);
    }

private Q_SLOTS:
    void init() {
        QVERIFY(m_root.isValid());
        m_storage = std::make_unique<kit::TemporaryStorage>(
            std::filesystem::path(m_root.filePath(QStringLiteral("temp")).toStdU16String()));
        m_launcher = std::make_unique<kit::ExitLauncher>();
        m_settings =
            std::make_unique<AppSettings>(m_root.filePath(QStringLiteral("settings.json")));
    }

    void cleanup() {
        m_settings.reset();
        m_launcher.reset();
        m_storage.reset();
    }

    void an_available_release_is_offered() {
        UpdateTask task(*m_settings, UpdateTask::Installed, m_root.filePath(QStringLiteral("app")));
        UpdateTaskView view(&task, *m_settings);
        QVERIFY(shows(view, "checkAgain"));
        task.offer(releaseOf(QStringLiteral("2.0")), QStringLiteral("app-setup.exe"));
        QVERIFY(shows(view, "download"));
        QVERIFY(shows(view, "skip"));
        QVERIFY(shows(view, "openPage"));
        QVERIFY(!shows(view, "installNow"));
        QVERIFY(!shows(view, "checkAgain"));
    }

    // A release for an application that does not install updates is offered for the page of the
    // release only, and preparing it does nothing.
    void an_unmanaged_release_is_offered_without_download() {
        UpdateTask task(*m_settings, UpdateTask::Unmanaged, m_root.filePath(QStringLiteral("app")));
        UpdateTaskView view(&task, *m_settings);
        QVERIFY(!task.installs());
        task.offer(releaseOf(QStringLiteral("2.0")), QStringLiteral("app-portable.tar.xz"));
        QVERIFY(!shows(view, "download"));
        QVERIFY(shows(view, "skip"));
        QVERIFY(shows(view, "openPage"));
        task.prepare();
        QCOMPARE(task.state(), UpdateTask::Available);
    }

    void skipping_records_the_version_and_leaves() {
        UpdateTask task(*m_settings, UpdateTask::Installed, m_root.filePath(QStringLiteral("app")));
        UpdateTaskView view(&task, *m_settings);
        task.offer(releaseOf(QStringLiteral("2.0")), QStringLiteral("app-setup.exe"));
        QSignalSpy leaving(&view, &UpdateTaskView::closeRequested);
        button(view, "skip")->click();
        QCOMPARE(m_settings->updates().skippedVersion(), QStringLiteral("2.0"));
        QCOMPARE(leaving.size(), 1);
    }

    // A release without the asset fails at once, and the failure offers to retry.
    void a_failure_offers_to_retry() {
        UpdateTask task(*m_settings, UpdateTask::Installed, m_root.filePath(QStringLiteral("app")));
        UpdateTaskView view(&task, *m_settings);
        task.offer(releaseOf(QStringLiteral("2.0")), QStringLiteral("app-setup.exe"));
        button(view, "download")->click();
        QCOMPARE(task.state(), UpdateTask::Failed);
        QVERIFY(shows(view, "retry"));
        QVERIFY(shows(view, "openPage"));
        QVERIFY(!shows(view, "download"));
    }

    // The installation on quit is listed among the programs that start at exit, where it can
    // be canceled.
    void an_installation_on_quit_is_listed_and_canceled() {
        UpdateTask task(*m_settings, UpdateTask::Installed, m_root.filePath(QStringLiteral("app")));
        UpdateTaskView view(&task, *m_settings);
        makeReady(task);
        QVERIFY(shows(view, "installNow"));
        QVERIFY(shows(view, "installOnQuit"));
        QVERIFY(!shows(view, "cancelInstall"));
        button(view, "installOnQuit")->click();
        QVERIFY(task.isScheduled());
        QVERIFY(!shows(view, "installNow"));
        QVERIFY(shows(view, "cancelInstall"));
        button(view, "cancelInstall")->click();
        QVERIFY(!task.isScheduled());
        QVERIFY(shows(view, "installOnQuit"));
        QVERIFY(!shows(view, "cancelInstall"));
    }

    void a_scheduled_restart_is_listed_and_canceled() {
        m_launcher->scheduleRestart();
        UpdateTask task(*m_settings, UpdateTask::Installed, m_root.filePath(QStringLiteral("app")));
        UpdateTaskView view(&task, *m_settings);
        QVERIFY(shows(view, "cancelRestart"));
        button(view, "cancelRestart")->click();
        QVERIFY(!m_launcher->isRestartScheduled());
        QVERIFY(!shows(view, "cancelRestart"));
    }

    void discarding_returns_to_the_offer() {
        UpdateTask task(*m_settings, UpdateTask::Installed, m_root.filePath(QStringLiteral("app")));
        UpdateTaskView view(&task, *m_settings);
        makeReady(task);
        button(view, "discard")->click();
        QCOMPARE(task.state(), UpdateTask::Available);
        QVERIFY(shows(view, "download"));
    }
};

QTEST_MAIN(test_UpdateTaskView)

#include "test_UpdateTaskView.moc"
