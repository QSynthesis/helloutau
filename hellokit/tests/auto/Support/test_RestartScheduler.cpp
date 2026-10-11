#include <QtCore/QCoreApplication>
#include <QtTest/QTest>

#include <hellokit/Support/RestartScheduler.h>

using namespace hello::kit;

class test_RestartScheduler : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // A required restart is recorded until it is cleared.
    void a_required_restart_is_recorded_until_cleared() {
        QVERIFY(!RestartScheduler::isRestartRequired());
        RestartScheduler::requireRestart();
        QVERIFY(RestartScheduler::isRestartRequired());
        RestartScheduler::clearRestartRequired();
        QVERIFY(!RestartScheduler::isRestartRequired());
    }

    // The program starts again once after scheduleRestart(), and not otherwise. The test
    // program started with -functions only lists its tests.
    void a_scheduled_restart_relaunches_once() {
        QVERIFY(!RestartScheduler::isRestartScheduled());
        QVERIFY(!RestartScheduler::relaunchIfScheduled({QStringLiteral("-functions")}));
        RestartScheduler::scheduleRestart();
        QVERIFY(RestartScheduler::isRestartScheduled());
        QVERIFY(RestartScheduler::relaunchIfScheduled({QStringLiteral("-functions")}));
        QVERIFY(!RestartScheduler::isRestartScheduled());
        QVERIFY(!RestartScheduler::relaunchIfScheduled({QStringLiteral("-functions")}));
    }
};

QTEST_GUILESS_MAIN(test_RestartScheduler)

#include "test_RestartScheduler.moc"
