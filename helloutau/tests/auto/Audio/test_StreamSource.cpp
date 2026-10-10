#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

#include <QtCore/QThread>
#include <QtTest/QTest>

#include <helloutau/Audio/SampleRateConversion.h>
#include <helloutau/Audio/StreamSource.h>

using namespace hello::daw;

class test_StreamSource : public QObject {
    Q_OBJECT

private Q_SLOTS:
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
        const auto whole = SampleRateConversion::converted(tone, 1, 44100, 48000);
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
};

QTEST_APPLESS_MAIN(test_StreamSource)

#include "test_StreamSource.moc"
