#include "AudioOutput.h"

#include <QtCore/QPointer>
#include <QtCore/QTimer>

#include <stdcorelib/pimpl.h>

#include "AudioEngine.h"
#include "DeviceClock.h"

namespace hello::daw {

    namespace {

        // How often the end of a source is looked for, in milliseconds
        constexpr int pollInterval = 20;

    }

    class AudioOutput::Impl {
    public:
        using Decl = AudioOutput;
        QPointer<AudioEngine> engine;
        std::optional<AudioMixer::SourceId> id;
        std::shared_ptr<DeviceClock> clock;
        QTimer poll;
        QTimer drain;
    };

    AudioOutput::AudioOutput(QObject *parent) : QObject(parent), _impl(std::make_unique<Impl>()) {
        stdc_impl_t;
        impl.engine = AudioEngine::instance();
        impl.poll.setInterval(pollInterval);
        impl.drain.setSingleShot(true);
        connect(&impl.drain, &QTimer::timeout, this, &AudioOutput::stop);
        connect(&impl.poll, &QTimer::timeout, this, [this] {
            stdc_impl_t;
            if (impl.id && impl.engine && impl.engine->isFinished(*impl.id)) {
                impl.poll.stop();
                impl.drain.start(impl.engine->bufferedMilliseconds() + pollInterval);
            }
        });
        connect(impl.engine, &AudioEngine::invalidated, this, [this](const QString &reason) {
            stdc_impl_t;
            if (impl.id) {
                stop();
                Q_EMIT failed(reason);
            }
        });
    }

    AudioOutput::~AudioOutput() {
        stdc_impl_t;
        if (impl.id && impl.engine) {
            impl.engine->stop(*impl.id);
        }
    }

    bool AudioOutput::start(std::shared_ptr<AudioSource> source, int sampleRate, QString *error) {
        return start(std::move(source), AudioEngine::instance()->deviceId(), sampleRate, error);
    }

    bool AudioOutput::start(std::shared_ptr<AudioSource> source, const QByteArray &id,
                            int sampleRate, QString *error) {
        stdc_impl_t;
        stop();
        impl.id = impl.engine->start(std::move(source), id, sampleRate, error);
        if (!impl.id) {
            return false;
        }
        impl.clock = impl.engine->clock(*impl.id);
        impl.poll.start();
        return true;
    }

    void AudioOutput::stop() {
        stdc_impl_t;
        impl.poll.stop();
        impl.drain.stop();
        if (!impl.id) {
            return;
        }
        if (impl.engine) {
            impl.engine->stop(*impl.id);
        }
        impl.id.reset();
        impl.clock.reset();
        Q_EMIT finished();
    }

    bool AudioOutput::isPlaying() const {
        stdc_impl_t;
        return impl.id.has_value();
    }

    std::optional<double> AudioOutput::heardPosition() const {
        stdc_impl_t;
        return impl.clock ? impl.clock->heard(DeviceClock::Clock::now()) : std::nullopt;
    }

}
