#include <algorithm>
#include <cmath>
#include <tuple>

#include <QtTest/QTest>

#include <hellokit/VoiceBank/FrequencyMap.h>

using namespace hello::kit;

namespace {

    FrequencyMap mapOf(std::initializer_list<double> frequencies) {
        FrequencyMap map;
        for (const double f : frequencies) {
            FrequencyMap::Frame frame;
            frame.frequency = f;
            map.frames.push_back(frame);
        }
        return map;
    }

    // The frequency a number of cents above A4
    double above440(double cents) {
        return 440 * std::exp2(cents / 1200);
    }

    // A map of frames at the times in milliseconds with the frequencies and the amplitudes
    FrequencyMap timedMapOf(std::initializer_list<std::tuple<double, double, double>> frames) {
        FrequencyMap map;
        map.hop = 10;
        for (const auto &[time, frequency, amplitude] : frames) {
            FrequencyMap::Frame frame;
            frame.time = time;
            frame.frequency = frequency;
            frame.amplitude = amplitude;
            map.frames.push_back(frame);
        }
        return map;
    }

    QList<double> frequenciesOf(const FrequencyMap &map) {
        QList<double> list;
        for (const auto &frame : map.frames) {
            list.push_back(frame.frequency);
        }
        return list;
    }

}

class test_FrequencyMap : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The average is taken on the pitch scale: two octaves average to the octave between them.
    // Frames at or below 55 Hz are left out.
    void the_average_is_taken_on_the_pitch_scale() {
        QCOMPARE(mapOf({220, 880}).pitchAverage(), 440.0);
        QCOMPARE(mapOf({0, 55, 220, 880}).pitchAverage(), 440.0);
    }

    // The notes and their mean are truncated to integral cents.
    void the_average_is_taken_in_integral_cents() {
        QCOMPARE(mapOf({above440(0.5)}).pitchAverage(), 440.0);
        QCOMPARE(mapOf({440, above440(1.5)}).pitchAverage(), 440.0);
        QCOMPARE(mapOf({above440(2.5)}).pitchAverage(), above440(2));
    }

    // Without frames above 55 Hz the average is C4.
    void the_average_without_voiced_frames_is_c4() {
        QCOMPARE(FrequencyMap().pitchAverage(), 261.6255653005986);
        QCOMPARE(mapOf({0, 55}).pitchAverage(), 261.6255653005986);
    }

    // The frequency of a frame between two voiced frames is interpolated, and the frequency of a
    // frame next to an unvoiced frame is that of the nearer frame. A frame after the last frame
    // has its values. The new frames are 4 ms apart at 1000 Hz.
    void a_resampled_frame_is_interpolated_between_voiced_frames() {
        auto map = timedMapOf({
            {0,  100, 1},
            {10, 200, 2},
            {20, 0,   3}
        });
        map.averageFrequency = 150;
        const auto resampled = map.resampled(4, 1000, 7);
        QCOMPARE(resampled.hop, 4.0);
        QCOMPARE(resampled.averageFrequency, std::optional<double>(150));
        QCOMPARE(resampled.frames.size(), size_t(7));
        QCOMPARE(resampled.frames.at(3).time, 12.0);
        QCOMPARE(frequenciesOf(resampled), (QList<double>{100, 140, 180, 200, 0, 0, 0}));
        QCOMPARE(*resampled.frames.at(1).amplitude, 1.4);
        QCOMPARE(*resampled.frames.at(3).amplitude, 2.2);
        QCOMPARE(*resampled.frames.at(6).amplitude, 3.0);
    }

    // A frame before the first frame has its values.
    void a_resampled_frame_before_the_first_frame_has_its_values() {
        const auto map = timedMapOf({
            {10, 100, 1},
            {20, 200, 2}
        });
        QCOMPARE(frequenciesOf(map.resampled(5, 1000, 3)), (QList<double>{100, 100, 100}));
    }

    // The amplitudes are left out unless every frame has one.
    void the_amplitudes_are_resampled_only_if_complete() {
        auto map = timedMapOf({
            {0,  100, 1},
            {10, 200, 2}
        });
        map.frames[1].amplitude.reset();
        const auto resampled = map.resampled(5, 1000, 3);
        QVERIFY(std::none_of(resampled.frames.begin(), resampled.frames.end(),
                             [](const auto &frame) { return frame.amplitude.has_value(); }));
    }
};

QTEST_GUILESS_MAIN(test_FrequencyMap)

#include "test_FrequencyMap.moc"
