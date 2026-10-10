#include <chrono>

#include <QtTest/QTest>

#include <helloutau/Audio/DeviceClock.h>

using namespace hello::daw;

class test_DeviceClock : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The device plays as time passes from its first pull, what it pulled earlier: the position
    // after the pull that holds the frame played, as far into it as the frame is, and still
    // across a pull of silence.
    void the_clock_finds_what_the_device_plays() {
        using namespace std::chrono_literals;
        // A frame a millisecond
        DeviceClock clock(1000);
        const auto t0 = DeviceClock::Clock::now();
        QVERIFY(!clock.heard(t0));

        // Pulled ahead: 100 frames of the source from 500 on, then 100 more
        clock.pulled(100, 500, 600, t0);
        clock.pulled(100, 600, 700, t0 + 5ms);
        QCOMPARE(*clock.heard(t0), 500.0);
        QCOMPARE(*clock.heard(t0 + 50ms), 550.0);
        QCOMPARE(*clock.heard(t0 + 150ms), 650.0);
        QCOMPARE(*clock.heard(t0 + 900ms), 700.0);

        // A pull of silence while the source is starved, then of the source again
        clock.pulled(100, 700, 700, t0 + 100ms);
        clock.pulled(100, 700, 800, t0 + 200ms);
        QCOMPARE(*clock.heard(t0 + 250ms), 700.0);
        QCOMPARE(*clock.heard(t0 + 350ms), 750.0);

        // Only the last pulls are kept, the older ones taken as the oldest kept.
        for (int i = 0; i < 200; ++i) {
            clock.pulled(10, 800 + i * 10, 810 + i * 10, t0 + 300ms);
        }
        QCOMPARE(*clock.heard(t0 + 2395ms), 2795.0);
        QVERIFY(*clock.heard(t0) >= 800);

        // Two frames a millisecond
        DeviceClock faster(2000);
        faster.pulled(100, 0, 100, t0);
        QCOMPARE(*faster.heard(t0 + 25ms), 50.0);
    }
};

QTEST_APPLESS_MAIN(test_DeviceClock)

#include "test_DeviceClock.moc"
