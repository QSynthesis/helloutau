#include <atomic>
#include <memory>

#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QThread>
#include <QtCore/QUuid>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <hellokit/Support/SingleProcessPeer.h>

using namespace hello::kit;

namespace {

    // Returns a key that no other run of the test uses
    QString uniqueKey() {
        return QStringLiteral("test_SingleProcessPeer-") +
               QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    // Sends from a later peer on another thread, because sendToPrimary() blocks while the
    // primary peer, on this thread, answers from the event loop. Returns the result once the
    // thread ends, while the event loop of this thread runs.
    bool sendFromAnotherThread(const QString &key, const QStringList &arguments,
                               const QString &workingDirectory, int timeout = 5000) {
        std::atomic<bool> sent = false;
        std::atomic<bool> primary = true;
        std::unique_ptr<QThread> thread(QThread::create([&] {
            SingleProcessPeer later(key);
            primary = later.isPrimary();
            sent = later.sendToPrimary(arguments, workingDirectory, timeout);
        }));
        thread->start();
        while (!thread->wait(10)) {
            QCoreApplication::processEvents();
        }
        return !primary && sent;
    }

}

class test_SingleProcessPeer : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The first peer of a key is the primary peer and receives the arguments and the working
    // directory of a later peer, which is not primary.
    void a_later_peer_sends_to_the_primary_peer() {
        const auto key = uniqueKey();
        SingleProcessPeer primary(key);
        QVERIFY(primary.isPrimary());
        QSignalSpy spy(&primary, &SingleProcessPeer::activateRequested);

        const QStringList arguments{QStringLiteral("a.ust"), QStringLiteral("你好.usth")};
        QVERIFY(sendFromAnotherThread(key, arguments, QStringLiteral("/work")));
        QTRY_COMPARE(spy.size(), 1);
        QCOMPARE(spy.at(0).at(0).toStringList(), arguments);
        QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("/work"));

        // The primary peer does not send.
        QVERIFY(!primary.sendToPrimary({}, {}));
    }

    // Peers of different keys are independent.
    void peers_of_different_keys_are_both_primary() {
        SingleProcessPeer first(uniqueKey());
        SingleProcessPeer second(uniqueKey());
        QVERIFY(first.isPrimary());
        QVERIFY(second.isPrimary());
    }

    // The next peer takes over once the primary peer ends.
    void the_next_peer_becomes_primary_after_the_primary_ends() {
        const auto key = uniqueKey();
        auto primary = std::make_unique<SingleProcessPeer>(key);
        QVERIFY(primary->isPrimary());
        primary.reset();
        SingleProcessPeer next(key);
        QVERIFY(next.isPrimary());
    }

    // Without a primary peer that answers, sending fails within the timeout.
    void sending_fails_without_an_answer() {
        const auto key = uniqueKey();
        SingleProcessPeer primary(key);
        // The event loop of the primary peer does not run during the send, so that no receipt
        // arrives.
        SingleProcessPeer later(key);
        QVERIFY(!later.isPrimary());
        QElapsedTimer elapsed;
        elapsed.start();
        QVERIFY(!later.sendToPrimary({QStringLiteral("a.ust")}, {}, 300));
        QVERIFY(elapsed.elapsed() < 5000);
    }
};

QTEST_GUILESS_MAIN(test_SingleProcessPeer)

#include "test_SingleProcessPeer.moc"
