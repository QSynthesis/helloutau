#include <cmath>

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
};

QTEST_GUILESS_MAIN(test_FrequencyMap)

#include "test_FrequencyMap.moc"
