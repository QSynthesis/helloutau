#include <QtTest/QTest>

#include <helloutau/Audio/AudioMixer.h>
#include <helloutau/Audio/BufferSource.h>

using namespace hello::daw;

class test_AudioMixer : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void mono_sources_reach_both_channels() {
        AudioMixer mixer(44100, 2);
        const auto id =
            mixer.add(std::make_shared<BufferSource>(std::vector<float>{0.25f, -0.5f}, 1));
        QVERIFY(id);
        float out[4] = {};
        mixer.render(out, 4);
        QCOMPARE(std::vector<float>(out, out + 4),
                 (std::vector<float>{0.25f, 0.25f, -0.5f, -0.5f}));
        mixer.render(out, 2);
        QVERIFY(mixer.isFinished(*id));
    }

    void sources_are_added_sample_by_sample() {
        AudioMixer mixer(44100, 2);
        QVERIFY(mixer.add(std::make_shared<BufferSource>(std::vector<float>{0.2f}, 1)));
        QVERIFY(mixer.add(std::make_shared<BufferSource>(std::vector<float>{0.3f}, 1)));
        float out[2] = {};
        mixer.render(out, 2);
        QCOMPARE(out[0], 0.5f);
        QCOMPARE(out[1], 0.5f);
    }

    void removing_one_source_leaves_the_other() {
        AudioMixer mixer(44100, 2);
        const auto first =
            mixer.add(std::make_shared<BufferSource>(std::vector<float>{0.2f, 0.2f}, 1));
        const auto second =
            mixer.add(std::make_shared<BufferSource>(std::vector<float>{0.3f, 0.3f}, 1));
        QVERIFY(first && second);
        mixer.remove(*first);
        float out[2] = {};
        mixer.render(out, 2);
        QCOMPARE(out[0], 0.3f);
        QCOMPARE(out[1], 0.3f);
    }
};

QTEST_APPLESS_MAIN(test_AudioMixer)

#include "test_AudioMixer.moc"
