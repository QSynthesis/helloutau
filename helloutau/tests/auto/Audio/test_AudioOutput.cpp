#include <atomic>
#include <cmath>

#include <QtCore/QThread>
#include <QtTest/QTest>

#include <helloutau/Audio/AudioOutput.h>
#include <helloutau/Audio/BufferSource.h>
#include <helloutau/Audio/StreamSource.h>

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

    // A shared buffer plays from the frame asked for, within its length.
    void a_shared_buffer_plays_from_a_frame() {
        const auto samples =
            std::make_shared<const std::vector<float>>(std::vector<float>{0.1f, 0.2f, 0.3f, 0.4f});
        BufferSource source(samples, 1, 2);
        QCOMPARE(source.position(), 2);
        float out[4] = {};
        QCOMPARE(source.read(out, 4, 1), 2);
        QCOMPARE(out[0], 0.3f);
        QCOMPARE(BufferSource(samples, 1, 9).position(), 4);
        QCOMPARE(BufferSource(samples, 1, -1).position(), 0);
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

    // Streamed through the ring, the tone arrives as the conversion of the whole would give it.
    void a_stream_arrives_converted() {
        constexpr double pi = 3.14159265358979323846;
        std::vector<float> tone(44100);
        for (size_t i = 0; i < tone.size(); ++i) {
            tone[i] = float(0.5 * std::sin(2 * pi * 440 * double(i) / 44100));
        }
        size_t next = 0;
        StreamSource source(
            [&](float *out, qsizetype frames) -> qsizetype {
                if (next >= tone.size()) {
                    return -1;
                }
                const auto count = std::min<size_t>(size_t(frames), tone.size() - next);
                std::copy_n(tone.begin() + qsizetype(next), count, out);
                next += count;
                return qsizetype(count);
            },
            44100, 48000);
        source.start();

        // One frame at a time, skipping the silence of a starved device
        std::vector<float> heard;
        for (int attempts = 0; attempts < 1000000; ++attempts) {
            float frame[2];
            const auto count = source.read(frame, 1, 2);
            if (count == 0) {
                break;
            }
            if (source.isStarved()) {
                QThread::usleep(200);
                continue;
            }
            QCOMPARE(frame[0], frame[1]);
            heard.push_back(frame[0]);
        }
        const auto whole = resampled(tone, 1, 44100, 48000);
        QCOMPARE(heard.size(), whole.size());
        double error = 0;
        for (size_t i = 0; i < heard.size(); ++i) {
            error = std::max(error, double(std::abs(heard[i] - whole[i])));
        }
        QVERIFY2(error < 1e-3, qPrintable(QString::number(error)));
        QCOMPARE(source.position(), qint64(44100));
    }

    // While the generator has nothing, the device plays silence and the position stands still.
    void a_starved_stream_waits() {
        std::atomic<bool> ready = false;
        std::atomic<int> given = 0;
        StreamSource source(
            [&](float *out, qsizetype frames) -> qsizetype {
                if (!ready.load()) {
                    QThread::msleep(1);
                    return 0;
                }
                if (given.load() >= 4410) {
                    return -1;
                }
                const auto count = std::min<qsizetype>(frames, 4410 - given.load());
                std::fill(out, out + count, 0.25f);
                given += int(count);
                return count;
            },
            44100, 44100);
        source.start();

        float frames[100];
        QCOMPARE(source.read(frames, 100, 1), 100);
        QVERIFY(source.isStarved());
        QCOMPARE(frames[0], 0.0f);
        QCOMPARE(source.position(), qint64(0));

        ready = true;
        bool arrived = false;
        for (int attempts = 0; attempts < 5000 && !arrived; ++attempts) {
            arrived = source.read(frames, 1, 1) == 1 && !source.isStarved();
            if (!arrived) {
                QThread::msleep(1);
            }
        }
        QVERIFY(arrived);
        QCOMPARE(frames[0], 0.25f);
        QCOMPARE(source.position(), qint64(1));

        // To the end, which reads fewer frames than requested
        qint64 total = 1;
        for (int attempts = 0; attempts < 100000; ++attempts) {
            const auto count = source.read(frames, 100, 1);
            if (!source.isStarved()) {
                total += count;
            }
            if (count < 100) {
                break;
            }
            if (source.isStarved()) {
                QThread::usleep(200);
            }
        }
        QCOMPARE(total, qint64(4410));
        QCOMPARE(source.position(), qint64(4410));
    }

    // Once stopped, a stream calls its generator no more, though it is still held and read.
    void a_stopped_stream_calls_its_generator_no_more() {
        std::atomic<int> calls = 0;
        auto source = std::make_shared<StreamSource>(
            [&calls](float *out, qsizetype frames) -> qsizetype {
                ++calls;
                std::fill(out, out + frames, 0.25f);
                return frames;
            },
            44100, 44100);
        source->start();
        for (int attempts = 0; attempts < 5000 && calls.load() == 0; ++attempts) {
            QThread::msleep(1);
        }
        QVERIFY(calls.load() > 0);

        source->stop();
        const int stopped = calls.load();
        float frames[4096];
        for (int i = 0; i < 20; ++i) {
            source->read(frames, 4096, 1);
            QThread::msleep(1);
        }
        QCOMPARE(calls.load(), stopped);
    }

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
