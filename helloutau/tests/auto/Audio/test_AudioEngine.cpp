#include <algorithm>
#include <atomic>
#include <memory>

#include <QtCore/QCoreApplication>
#include <QtMultimedia/QMediaDevices>
#include <QtTest/QTest>

#include <helloutau/Audio/AudioEngine.h>

using namespace hello::daw;

namespace {

    // A silent source without end
    class SilentSource : public AudioSource {
    public:
        qsizetype read(float *out, qsizetype frames, int channels) noexcept override {
            std::fill_n(out, frames * channels, 0.0f);
            m_position += frames;
            return frames;
        }

        qint64 position() const noexcept override {
            return m_position;
        }

    private:
        std::atomic<qint64> m_position = 0;
    };

}

class test_AudioEngine : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void is_a_process_wide_singleton() {
        QCOMPARE(AudioEngine::instance(), AudioEngine::instance());
    }

    void keeps_the_selected_device_id_without_opening_a_stream() {
        auto engine = AudioEngine::instance();
        engine->setDeviceId(QByteArrayLiteral("test-device"));
        QCOMPARE(engine->deviceId(), QByteArrayLiteral("test-device"));
        QCOMPARE(engine->bufferedMilliseconds(), 0);
        engine->setDeviceId({});
    }

    void reports_empty_sources_as_finished() {
        auto engine = AudioEngine::instance();
        QVERIFY(engine->isFinished(0));
        QVERIFY(!engine->clock(0));
        QCOMPARE(engine->bufferedMilliseconds(), 0);
    }

    void lists_the_output_devices() {
        QList<QByteArray> ids;
        for (const auto &device : QMediaDevices::audioOutputs()) {
            ids.push_back(device.id());
            QCOMPARE(AudioEngine::deviceDescription(device.id()), device.description());
        }
        QCOMPARE(AudioEngine::deviceIds(), ids);
        QVERIFY(AudioEngine::deviceDescription(QByteArrayLiteral("no-such-device")).isEmpty());
    }

    // Stopping a source releases it. The stream stays open while another source plays, and is
    // closed after the last source stops.
    void the_stream_is_closed_after_the_last_source_stops() {
        auto engine = AudioEngine::instance();
        if (engine->sampleRate() == 0) {
            QSKIP("No audio output device exists.");
        }
        QString error;
        const auto first =
            engine->start(std::make_shared<SilentSource>(), engine->sampleRate(), &error);
        QVERIFY2(first, qPrintable(error));
        const auto second =
            engine->start(std::make_shared<SilentSource>(), engine->sampleRate(), &error);
        QVERIFY2(second, qPrintable(error));
        QVERIFY(engine->clock(*first));
        QVERIFY(engine->clock(*second));

        engine->stop(*first);
        QVERIFY(engine->isFinished(*first));
        QVERIFY(!engine->clock(*first));
        QVERIFY(!engine->isFinished(*second));
        QVERIFY(engine->clock(*second));

        engine->stop(*second);
        QVERIFY(engine->isFinished(*second));
        QVERIFY(!engine->clock(*second));
        QCOMPARE(engine->bufferedMilliseconds(), 0);
    }
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    test_AudioEngine test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_AudioEngine.moc"
