#include <QtCore/QTemporaryDir>
#include <QtCore/QTimer>
#include <QtTest/QTest>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>

#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/ProjectWindow.h>
#include <helloutau/Editor/Restarter.h>

using namespace hello::daw;

class test_Restarter : public QObject {
    Q_OBJECT

    // Answers the question of offer() with \a button once it is shown
    static void answer(QMessageBox::StandardButton button, bool *asked) {
        QTimer::singleShot(0, [button, asked] {
            const auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            *asked = true;
            box->button(button)->click();
        });
    }

private Q_SLOTS:
    // Without a setting that needs it, nothing is asked and nothing restarts.
    void nothing_is_offered_unless_needed() {
        QTemporaryDir dir;
        Editor editor(std::make_unique<AppSettings>(dir.filePath(QStringLiteral("s.json"))));
        QVERIFY(!Restarter::isNeeded());
        QVERIFY(!Restarter::offer(nullptr, &editor));
        QVERIFY(!Restarter::isRestarting());
        QVERIFY(!Restarter::startAgain({}));
    }

    // Accepted, the restart closes the windows and starts the program again with the arguments
    // once; the test program started with -functions only lists its tests.
    void an_accepted_restart_closes_the_windows_and_starts_again() {
        QTemporaryDir dir;
        Editor editor(std::make_unique<AppSettings>(dir.filePath(QStringLiteral("s.json"))));
        editor.setWatchesDisk(false);
        const auto window = editor.newWindow();
        window->show();

        Restarter::markNeeded();
        QVERIFY(Restarter::isNeeded());
        bool asked = false;
        answer(QMessageBox::Yes, &asked);
        QVERIFY(Restarter::offer(nullptr, &editor));
        QVERIFY(asked);
        QVERIFY(!Restarter::isNeeded());
        QVERIFY(Restarter::isRestarting());
        QVERIFY(!window->isVisible());

        QVERIFY(Restarter::startAgain({QStringLiteral("-functions")}));
        QVERIFY(!Restarter::isRestarting());
        QVERIFY(!Restarter::startAgain({QStringLiteral("-functions")}));
    }
};

QTEST_MAIN(test_Restarter)

#include "test_Restarter.moc"
