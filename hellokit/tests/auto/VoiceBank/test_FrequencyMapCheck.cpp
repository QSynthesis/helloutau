#include <cmath>
#include <initializer_list>
#include <utility>

#include <QtTest/QTest>

#include <hellokit/VoiceBank/FrequencyMapCheck.h>

using namespace hello::kit;

namespace {

    using Check = FrequencyMapCheck;

    // A map of frames given as frequency and amplitude, 5 milliseconds apart
    FrequencyMap mapOf(std::initializer_list<std::pair<double, double>> frames) {
        FrequencyMap map;
        map.hop = 256;
        double time = 0;
        for (const auto &[frequency, amplitude] : frames) {
            FrequencyMap::Frame frame;
            frame.time = time;
            frame.frequency = frequency;
            frame.amplitude = amplitude;
            map.frames.push_back(frame);
            time += 5;
        }
        return map;
    }

    // A map of loud frames of the given frequencies
    FrequencyMap loudMapOf(std::initializer_list<double> frequencies) {
        FrequencyMap map;
        map.hop = 256;
        double time = 0;
        for (const double frequency : frequencies) {
            FrequencyMap::Frame frame;
            frame.time = time;
            frame.frequency = frequency;
            frame.amplitude = 1000;
            map.frames.push_back(frame);
            time += 5;
        }
        return map;
    }

    double frequencyOf(double note) {
        return std::pow(2.0, (note - 69.0) / 12.0) * 440.0;
    }

    bool near(double a, double b) {
        return std::abs(a - b) < 1e-9 * std::max(1.0, std::abs(b));
    }

}

class test_FrequencyMapCheck : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The threshold is the louder frame of each pair at a quarter of the ascending order,
    // limited to 500 and 2000.
    void the_threshold_of_the_segments() {
        QCOMPARE(Check::segmentThreshold(mapOf({
                     {200, 100 },
                     {200, 3000},
                     {200, 200 },
                     {200, 400 },
                     {200, 1000},
                     {200, 800 },
                     {200, 5000},
                     {200, 0   }
        })),
                 1000.0);
        QCOMPARE(Check::segmentThreshold(mapOf({
                     {200, 10},
                     {200, 20}
        })),
                 500.0);
        QCOMPARE(Check::segmentThreshold(mapOf({
                     {200, 9000},
                     {200, 9000}
        })),
                 2000.0);
        QCOMPARE(Check::segmentThreshold(mapOf({
                     {200, 9000}
        })),
                 0.0);
    }

    // A loud pair starts a segment, which extends over the voiced frames next to it. A voiced
    // frame apart from it is in no segment.
    void the_segments_extend_over_the_voiced_frames() {
        const auto map = mapOf({
            {0,   0   },
            {0,   0   },
            {200, 0   },
            {200, 0   },
            {200, 1000},
            {200, 1000},
            {0,   0   },
            {200, 0   },
            {0,   0   },
            {0,   0   }
        });
        const auto segments = Check::segments(map);
        QCOMPARE(segments.size(), size_t(1));
        QCOMPARE(segments.front(), (Check::Run{2, 5}));
    }

    // A frame an octave above the range gets half its frequency, the multiple nearest to the
    // frames around it. The frames inside the range get no correction.
    void a_frame_out_of_range_gets_a_multiple() {
        const auto corrections = Check::check(loudMapOf({260, 260, 520, 260, 260}), {});
        QCOMPARE(corrections.size(), size_t(5));
        QCOMPARE(corrections[2].reason, Check::OutOfRange);
        QCOMPARE(corrections[2].frequency, 260.0);
        for (const size_t i : {0, 1, 3, 4}) {
            QCOMPARE(corrections[i].reason, Check::NoCorrection);
        }
    }

    // A run at the end of a segment takes the note of the settings at that end.
    void a_run_at_the_end_takes_the_note() {
        const auto corrections = Check::check(loudMapOf({260, 260, 1040}), {});
        QCOMPARE(corrections[2].reason, Check::OutOfRange);
        QCOMPARE(corrections[2].frequency, 260.0);
    }

    // The unvoiced frames between voiced frames get frequencies interpolated on the pitch scale
    // between the frames around them.
    void unvoiced_frames_between_voiced_frames_are_filled() {
        const auto corrections = Check::check(loudMapOf({220, 0, 0, frequencyOf(59)}), {});
        QCOMPARE(corrections[0].reason, Check::NoCorrection);
        QCOMPARE(corrections[3].reason, Check::NoCorrection);
        for (const size_t i : {1, 2}) {
            QCOMPARE(corrections[i].reason, Check::UnvoicedFrame);
            QVERIFY2(near(corrections[i].frequency, frequencyOf(57 + double(i) * 2 / 3)),
                     qPrintable(QString::number(corrections[i].frequency)));
        }
    }

    // A loud segment without a voiced frame takes the note of the settings throughout.
    void a_segment_without_voiced_frames_takes_the_note() {
        Check::Settings settings;
        settings.note = 64;
        const auto corrections = Check::check(loudMapOf({0, 0, 0}), settings);
        for (const auto &correction : corrections) {
            QCOMPARE(correction.reason, Check::UnvoicedFrame);
            QVERIFY(near(correction.frequency, frequencyOf(64)));
        }
    }

    // A change of pitch widens the range from 5 to 10 semitones.
    void a_change_of_pitch_widens_the_range() {
        const auto map = loudMapOf({260, 400, 260});
        QCOMPARE(Check::check(map, {}).at(1).reason, Check::OutOfRange);
        Check::Settings settings;
        settings.allowsPitchChange = true;
        QCOMPARE(Check::check(map, settings).at(1).reason, Check::NoCorrection);
    }

    // A change at the start or at the end of a segment keeps its frames as they are.
    void a_change_at_either_end_is_kept() {
        const auto start = loudMapOf({520, 520, 260, 260});
        QCOMPARE(Check::check(start, {}).at(0).reason, Check::OutOfRange);
        Check::Settings settings;
        settings.allowsChangeAtStart = true;
        for (const auto &correction : Check::check(start, settings)) {
            QCOMPARE(correction.reason, Check::NoCorrection);
        }

        const auto end = loudMapOf({260, 260, 520, 520});
        QCOMPARE(Check::check(end, {}).at(3).reason, Check::OutOfRange);
        settings = {};
        settings.allowsChangeAtEnd = true;
        for (const auto &correction : Check::check(end, settings)) {
            QCOMPARE(correction.reason, Check::NoCorrection);
        }
    }

    // A frame without an amplitude gets the root mean square of the first channel over its hop,
    // in 16-bit units, or 0 beyond the audio. A frame with an amplitude keeps it.
    void amplitudes_from_the_audio() {
        WaveAudio audio;
        audio.sampleRate = 1000;
        audio.channels = 2;
        for (const float sample : {0.5f, -0.5f, 0.25f, 0.25f}) {
            audio.samples.push_back(sample);
            audio.samples.push_back(1.0f);
        }
        FrequencyMap map;
        map.hop = 2;
        map.frames.resize(4);
        map.frames[2].amplitude = 7;
        const auto result = Check::withAmplitudes(map, audio);
        QCOMPARE(result.frames[0].amplitude, std::optional<double>(16384));
        QCOMPARE(result.frames[1].amplitude, std::optional<double>(8192));
        QCOMPARE(result.frames[2].amplitude, std::optional<double>(7));
        QCOMPARE(result.frames[3].amplitude, std::optional<double>(0));
    }

    // Frames outside every segment are not checked.
    void quiet_frames_are_not_checked() {
        const auto corrections =
            Check::check(mapOf({
                             {260,  1000},
                             {260,  1000},
                             {0,    0   },
                             {0,    0   },
                             {2000, 0   },
                             {0,    0   }
        }),
                         {});
        QCOMPARE(corrections[4].reason, Check::NoCorrection);
    }
};

QTEST_APPLESS_MAIN(test_FrequencyMapCheck)

#include "test_FrequencyMapCheck.moc"
