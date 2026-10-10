#include <algorithm>
#include <cmath>
#include <vector>

#include <QtTest/QTest>

#include <helloutau/Audio/SampleRateConversion.h>

using namespace hello::daw;

class test_SampleRateConversion : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // A tone keeps its pitch and its duration across a change of the sample rate.
    void resampling_keeps_pitch_and_duration() {
        constexpr double pi = 3.14159265358979323846;
        constexpr int from = 44100;
        constexpr int to = 48000;
        constexpr double frequency = 440;
        std::vector<float> tone(from);
        for (size_t i = 0; i < tone.size(); ++i) {
            tone[i] = float(0.5 * std::sin(2 * pi * frequency * double(i) / from));
        }

        const auto converted = SampleRateConversion::converted(tone, 1, from, to);
        QCOMPARE(qsizetype(converted.size()), qsizetype(to));
        // Away from the edges, where the filter has no neighbors, the result is the same tone.
        double error = 0;
        for (int i = to / 4; i < to * 3 / 4; ++i) {
            const double expected = 0.5 * std::sin(2 * pi * frequency * i / to);
            error = std::max(error, std::abs(converted[size_t(i)] - expected));
        }
        QVERIFY2(error < 1e-3, qPrintable(QString::number(error)));

        // The same rate, or nothing, passes through.
        QCOMPARE(SampleRateConversion::converted(tone, 1, from, from), tone);
        QVERIFY(SampleRateConversion::converted({}, 1, from, to).empty());
    }

    // Interleaved channels are converted separately.
    void channels_are_resampled_separately() {
        std::vector<float> stereo;
        for (int i = 0; i < 4410; ++i) {
            stereo.push_back(0.25f);
            stereo.push_back(-0.5f);
        }
        const auto converted = SampleRateConversion::converted(stereo, 2, 44100, 22050);
        QCOMPARE(qsizetype(converted.size()), qsizetype(2 * 2205));
        QVERIFY(std::abs(converted[2 * 1000] - 0.25f) < 1e-3);
        QVERIFY(std::abs(converted[2 * 1000 + 1] + 0.5f) < 1e-3);
    }
};

QTEST_APPLESS_MAIN(test_SampleRateConversion)

#include "test_SampleRateConversion.moc"
