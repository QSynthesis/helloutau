#include <vector>

#include <QtTest/QTest>

#include <helloutau/Audio/BufferSource.h>

using namespace hello::daw;

class test_BufferSource : public QObject {
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
};

QTEST_APPLESS_MAIN(test_BufferSource)

#include "test_BufferSource.moc"
