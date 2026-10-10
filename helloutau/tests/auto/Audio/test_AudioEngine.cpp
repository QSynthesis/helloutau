#include <QtCore/QCoreApplication>
#include <QtTest/QTest>

#include <helloutau/Audio/AudioEngine.h>

using namespace hello::daw;

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
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    test_AudioEngine test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_AudioEngine.moc"
