#include <algorithm>
#include <functional>
#include <optional>

#include <QtTest/QTest>

#include <helloutau/Audio/AudioMixer.h>
#include <helloutau/Audio/BufferSource.h>

using namespace hello::daw;

namespace {

    // A silent source without end. Each read() calls the function given to the constructor.
    class CallbackSource : public AudioSource {
    public:
        explicit CallbackSource(std::function<void()> onRead = {}) : m_onRead(std::move(onRead)) {
        }

        qsizetype read(float *out, qsizetype frames, int channels) noexcept override {
            std::fill_n(out, frames * channels, 0.0f);
            m_position += frames;
            if (m_onRead) {
                m_onRead();
            }
            return frames;
        }

        qint64 position() const noexcept override {
            return m_position;
        }

    private:
        std::function<void()> m_onRead;
        qint64 m_position = 0;
    };

}

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

    // Ids are unique across mixers, so that an id of an earlier mixer does not name a source of a
    // later mixer.
    void ids_of_an_earlier_mixer_do_not_name_sources_of_a_later_mixer() {
        std::optional<AudioMixer::SourceId> earlier;
        {
            AudioMixer mixer(44100, 1);
            earlier = mixer.add(std::make_shared<CallbackSource>());
            QVERIFY(earlier);
        }
        AudioMixer mixer(44100, 1);
        const auto later = mixer.add(std::make_shared<CallbackSource>());
        QVERIFY(later);
        QVERIFY(*later != *earlier);
        QVERIFY(mixer.isFinished(*earlier));
        mixer.remove(*earlier);
        QVERIFY(!mixer.isFinished(*later));
        QVERIFY(!mixer.isIdle());
    }

    // A mixer is idle without sources, and after its sources have ended or have been removed.
    void a_mixer_is_idle_without_playing_sources() {
        AudioMixer mixer(44100, 1);
        QVERIFY(mixer.isIdle());
        const auto id = mixer.add(std::make_shared<BufferSource>(std::vector<float>{0.5f}, 1));
        QVERIFY(id);
        QVERIFY(!mixer.isIdle());
        float out[2] = {};
        mixer.render(out, 2);
        QVERIFY(mixer.isFinished(*id));
        QVERIFY(mixer.isIdle());
        mixer.collect();
        QVERIFY(mixer.isIdle());

        const auto endless = mixer.add(std::make_shared<CallbackSource>());
        QVERIFY(endless);
        mixer.render(out, 2);
        QVERIFY(!mixer.isIdle());
        mixer.remove(*endless);
        QVERIFY(mixer.isFinished(*endless));
        QVERIFY(mixer.isIdle());
    }

    // A source whose removal is requested during its read is no longer playing, although the
    // read has not finished.
    void a_source_removed_during_its_read_is_not_playing() {
        AudioMixer mixer(44100, 1);
        std::optional<AudioMixer::SourceId> id;
        bool read = false;
        bool idleInRead = false;
        bool finishedInRead = false;
        id = mixer.add(std::make_shared<CallbackSource>([&] {
            if (!read) {
                read = true;
                mixer.remove(*id);
                idleInRead = mixer.isIdle();
                finishedInRead = mixer.isFinished(*id);
            }
        }));
        QVERIFY(id);
        float out[1] = {};
        mixer.render(out, 1);
        QVERIFY(read);
        QVERIFY(idleInRead);
        QVERIFY(finishedInRead);
        QVERIFY(mixer.isFinished(*id));
        QVERIFY(mixer.isIdle());
    }
};

QTEST_APPLESS_MAIN(test_AudioMixer)

#include "test_AudioMixer.moc"
