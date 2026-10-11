#include <filesystem>

#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>
#include <QtWidgets/QPushButton>

#include <hellokit/Support/TemporaryStorage.h>

#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Editor/Dialogs/UpdateDialog.h>
#include <helloutau/Editor/UpdateTask.h>
#include <helloutau/Editor/UpdateTaskView.h>

using namespace hello;
using namespace hello::daw;

class test_UpdateDialog : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The dialog closes when the user leaves its view, and the task runs on.
    void leaving_the_view_closes_the_dialog() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        kit::TemporaryStorage storage(
            std::filesystem::path(dir.filePath(QStringLiteral("temp")).toStdU16String()));
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        UpdateTask task(settings, UpdateTask::Portable, dir.filePath(QStringLiteral("app")));
        UpdateDialog dialog(&task, settings);
        QVERIFY(dialog.view());
        QVERIFY(!dialog.windowTitle().isEmpty());
        dialog.show();
        dialog.view()->findChild<QPushButton *>(QStringLiteral("close"))->click();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QVERIFY(!dialog.isVisible());
    }
};

QTEST_MAIN(test_UpdateDialog)

#include "test_UpdateDialog.moc"
