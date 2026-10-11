#include <QtCore/QCoreApplication>
#include <QtTest/QTest>

#include <hellokit/Support/ExitLauncher.h>

using namespace hello::kit;

class test_ExitLauncher : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void the_instance_exists_as_long_as_the_launcher() {
        QVERIFY(!ExitLauncher::instance());
        {
            ExitLauncher launcher;
            QCOMPARE(ExitLauncher::instance(), &launcher);
        }
        QVERIFY(!ExitLauncher::instance());
    }

    // A launch scheduled again under its key replaces the earlier one, and a removed launch
    // does not start. The test program started with -functions only lists its tests.
    void each_key_starts_once() {
        const auto self = QCoreApplication::applicationFilePath();
        const QStringList arguments{QStringLiteral("-functions")};
        ExitLauncher launcher;
        launcher.setLaunch(QStringLiteral("a"), self, arguments);
        launcher.setLaunch(QStringLiteral("a"), self, arguments);
        launcher.setLaunch(QStringLiteral("b"), self, arguments);
        launcher.setLaunch(QStringLiteral("c"), self, arguments);
        launcher.removeLaunch(QStringLiteral("c"));
        QVERIFY(launcher.hasLaunch(QStringLiteral("a")));
        QVERIFY(!launcher.hasLaunch(QStringLiteral("c")));
        QCOMPARE(launcher.launchAll(), 2);
        QVERIFY(!launcher.hasLaunch(QStringLiteral("a")));
        QCOMPARE(launcher.launchAll(), 0);
    }

    // The restart is a launch of the program itself under its key.
    void a_restart_is_a_launch_of_the_program() {
        ExitLauncher launcher({QStringLiteral("-functions")});
        QVERIFY(!launcher.isRestartScheduled());
        launcher.scheduleRestart();
        QVERIFY(launcher.isRestartScheduled());
        QVERIFY(launcher.hasLaunch(QLatin1String(ExitLauncher::restartKey)));
        QCOMPARE(launcher.launchAll(), 1);
        QVERIFY(!launcher.isRestartScheduled());
    }

    void a_required_restart_is_recorded_until_cleared() {
        ExitLauncher launcher;
        QVERIFY(!launcher.isRestartRequired());
        launcher.requireRestart();
        QVERIFY(launcher.isRestartRequired());
        launcher.clearRestartRequired();
        QVERIFY(!launcher.isRestartRequired());
    }
};

QTEST_GUILESS_MAIN(test_ExitLauncher)

#include "test_ExitLauncher.moc"
