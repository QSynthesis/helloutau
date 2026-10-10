#include <algorithm>
#include <cmath>
#include <vector>

#include <QtTest/QTest>

#include <helloutau/Audio/PianoToneSource.h>

using namespace hello::daw;

class test_PianoToneSource : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The procedural piano source is finite, audible, and mono on every channel.
    void a_piano_tone_is_finite_and_mono() {
        PianoToneSource source(48000, 440, 0.1);
        std::vector<float> out(480 * 2);
        QCOMPARE(source.read(out.data(), 480, 2), 480);
        QCOMPARE(source.position(), 480);
        QVERIFY(std::any_of(out.cbegin(), out.cend(),
                            [](float sample) { return std::abs(sample) > 1e-4f; }));
        QCOMPARE(out[100], out[101]);
        std::vector<float> rest(48000 * 2);
        while (source.read(rest.data(), 48000, 2) > 0) {
        }
        QCOMPARE(source.read(rest.data(), 1, 2), 0);
    }
};

QTEST_APPLESS_MAIN(test_PianoToneSource)

#include "test_PianoToneSource.moc"
