#include <cmath>

#include <QtTest/QTest>

#include <helloutau/Audio/AudioOutput.h>

using namespace hello::daw;

class test_AudioOutput : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // A mono buffer plays on every channel, and the end of the buffer ends the source.
    void a_buffer_plays_to_its_end() {
        BufferSource source({0.1f, 0.2f, 0.3f}, 1);
        QCOMPARE(source.frameCount(), 3);

        float out[4] = {};
        QCOMPARE(source.read(out, 2, 2), 2);
        QCOMPARE(std::vector<float>(out, out + 4), (std::vector<float>{0.1f, 0.1f, 0.2f, 0.2f}));
        QCOMPARE(source.position(), 2);

        QCOMPARE(source.read(out, 2, 2), 1);
        QCOMPARE(out[0], 0.3f);
        QCOMPARE(source.read(out, 2, 2), 0);
    }

    // A stereo buffer plays channel by channel: on a mono device its first channel, on a device
    // of more channels silence beyond its own.
    void a_stereo_buffer_follows_the_channels_of_the_device() {
        BufferSource mono({0.1f, 0.2f, 0.3f, 0.4f}, 2);
        float out[6] = {};
        QCOMPARE(mono.read(out, 2, 1), 2);
        QCOMPARE(std::vector<float>(out, out + 2), (std::vector<float>{0.1f, 0.3f}));

        BufferSource wide({0.1f, 0.2f}, 2);
        QCOMPARE(wide.read(out, 1, 3), 1);
        QCOMPARE(std::vector<float>(out, out + 3), (std::vector<float>{0.1f, 0.2f, 0.0f}));
    }

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

        const auto converted = resampled(tone, 1, from, to);
        QCOMPARE(qsizetype(converted.size()), qsizetype(to));
        // Away from the edges, where the filter has no neighbors, the result is the same tone.
        double error = 0;
        for (int i = to / 4; i < to * 3 / 4; ++i) {
            const double expected = 0.5 * std::sin(2 * pi * frequency * i / to);
            error = std::max(error, std::abs(converted[size_t(i)] - expected));
        }
        QVERIFY2(error < 1e-3, qPrintable(QString::number(error)));

        // The same rate, or nothing, passes through.
        QCOMPARE(resampled(tone, 1, from, from), tone);
        QVERIFY(resampled({}, 1, from, to).empty());
    }

    // Interleaved channels are converted separately.
    void channels_are_resampled_separately() {
        std::vector<float> stereo;
        for (int i = 0; i < 4410; ++i) {
            stereo.push_back(0.25f);
            stereo.push_back(-0.5f);
        }
        const auto converted = resampled(stereo, 2, 44100, 22050);
        QCOMPARE(qsizetype(converted.size()), qsizetype(2 * 2205));
        QVERIFY(std::abs(converted[2 * 1000] - 0.25f) < 1e-3);
        QVERIFY(std::abs(converted[2 * 1000 + 1] + 0.5f) < 1e-3);
    }
};

QTEST_APPLESS_MAIN(test_AudioOutput)

#include "test_AudioOutput.moc"
