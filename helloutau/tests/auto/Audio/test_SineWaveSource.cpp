#include <algorithm>
#include <cmath>
#include <vector>

#include <QtTest/QTest>

#include <helloutau/Audio/SineWaveSource.h>

using namespace hello::daw;

class test_SineWaveSource : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // A sample rate that is not positive is taken as 1, so that the duration is still counted in
    // frames and every sample is finite.
    void a_sample_rate_that_is_not_positive_is_one() {
        for (const int rate : {0, -48000}) {
            SineWaveSource source(rate, 0.25, 8, 0.5);
            std::vector<float> out(16);
            QCOMPARE(source.read(out.data(), 16, 1), 8);
            QVERIFY(std::all_of(out.cbegin(), out.cend(),
                                [](float sample) { return std::isfinite(sample); }));
        }
    }

    void a_negative_amplitude_is_silent() {
        SineWaveSource source(48000, 440, 0.1, -0.5);
        std::vector<float> out(4800);
        QCOMPARE(source.read(out.data(), 4800, 1), 4800);
        QVERIFY(std::all_of(out.cbegin(), out.cend(), [](float sample) { return sample == 0; }));
    }

    // The tone fades in from silence and fades out to silence, and is at full amplitude between.
    void the_tone_fades_in_and_out() {
        SineWaveSource source(48000, 12000, 1, 0.5);
        std::vector<float> out(48000 * 2);
        QCOMPARE(source.read(out.data(), 48000, 2), 48000);
        QCOMPARE(out[0], 0.0f);
        QVERIFY(std::abs(out[2 * 1]) < 0.01f);
        QVERIFY(std::abs(std::abs(out[2 * 24001]) - 0.5f) < 1e-4f);
        QVERIFY(std::abs(out[2 * 47999]) < 0.01f);
        QCOMPARE(out[2 * 24001], out[2 * 24001 + 1]);
    }
};

QTEST_APPLESS_MAIN(test_SineWaveSource)

#include "test_SineWaveSource.moc"
