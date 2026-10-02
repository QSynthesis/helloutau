#include "AudioEngine.h"

#include <cstring>

#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtMultimedia/QAudioSink>
#include <QtMultimedia/QMediaDevices>

#include <stdcorelib/pimpl.h>

namespace hello::daw {

    namespace {
#if QT_VERSION < QT_VERSION_CHECK(6, 11, 0) || defined(HELLOUTAU_AUDIO_PULL)
#  define HELLOUTAU_AUDIO_PULL_DEVICE
        // Retains partial frame bytes across QIODevice reads, including unaligned requests.
        class MixerDevice : public QIODevice {
        public:
            MixerDevice(AudioMixer &mixer, int channels)
                : m_mixer(mixer), m_frame(size_t(channels)) {
                open(QIODevice::ReadOnly | QIODevice::Unbuffered);
            }
            bool isSequential() const override {
                return true;
            }
            qint64 bytesAvailable() const override {
                return 4096 + QIODevice::bytesAvailable();
            }

        protected:
            qint64 readData(char *out, qint64 size) override {
                qint64 copied = 0;
                const auto bytes = qint64(m_frame.size() * sizeof(float));
                while (copied < size) {
                    if (m_remaining == 0) {
                        m_mixer.render(m_frame.data(), qsizetype(m_frame.size()));
                        m_remaining = bytes;
                    }
                    const auto count = std::min(size - copied, m_remaining);
                    std::memcpy(out + copied,
                                reinterpret_cast<const char *>(m_frame.data()) + bytes -
                                    m_remaining,
                                size_t(count));
                    m_remaining -= count;
                    copied += count;
                }
                return copied;
            }
            qint64 writeData(const char *, qint64) override {
                return -1;
            }

        private:
            AudioMixer &m_mixer;
            std::vector<float> m_frame;
            qint64 m_remaining = 0;
        };
#endif
    }

    class AudioEngine::Impl {
    public:
        using Decl = AudioEngine;
        QByteArray id;
        QAudioDevice opened;
        std::unique_ptr<AudioMixer> mixer;
#ifdef HELLOUTAU_AUDIO_PULL_DEVICE
        std::unique_ptr<MixerDevice> device;
#endif
        std::unique_ptr<QAudioSink> sink;
        QMediaDevices devices;
        QTimer poll;
        quint64 generation = 0;
    };

    AudioEngine *AudioEngine::instance() {
        Q_ASSERT(QCoreApplication::instance());
        Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
        static QPointer<AudioEngine> engine;
        if (!engine) {
            engine = new AudioEngine(QCoreApplication::instance());
        }
        return engine;
    }

    AudioEngine::AudioEngine(QObject *parent) : QObject(parent), _impl(std::make_unique<Impl>()) {
        stdc_impl_t;
        impl.poll.setInterval(20);
        connect(&impl.poll, &QTimer::timeout, this, [this] {
            stdc_impl_t;
            if (impl.sink && impl.sink->error() != QtAudio::NoError &&
                impl.sink->state() == QtAudio::StoppedState) {
                close();
                Q_EMIT invalidated(tr("The audio output device stopped unexpectedly."));
            }
            if (impl.mixer) {
                impl.mixer->collect();
            }
        });
        connect(&impl.devices, &QMediaDevices::audioOutputsChanged, this, [this] {
            stdc_impl_t;
            if (impl.sink && device() != impl.opened) {
                close();
                Q_EMIT invalidated(tr("The audio output device changed. Start playback again."));
            }
            Q_EMIT devicesChanged();
        });
    }

    AudioEngine::~AudioEngine() {
        close();
    }

    QByteArray AudioEngine::deviceId() const {
        stdc_impl_t;
        return impl.id;
    }

    QAudioDevice AudioEngine::device() const {
        stdc_impl_t;
        if (impl.id.isEmpty()) {
            return QMediaDevices::defaultAudioOutput();
        }
        for (const auto &device : QMediaDevices::audioOutputs()) {
            if (device.id() == impl.id) {
                return device;
            }
        }
        return QMediaDevices::defaultAudioOutput();
    }

    int AudioEngine::sampleRate() const {
        const auto selected = device();
        return selected.isNull() ? 0 : selected.preferredFormat().sampleRate();
    }

    void AudioEngine::setDeviceId(const QByteArray &id) {
        stdc_impl_t;
        if (impl.id == id) {
            return;
        }
        close();
        impl.id = id;
        Q_EMIT invalidated(tr("The audio output device changed. Start playback again."));
    }

    void AudioEngine::close() {
        stdc_impl_t;
        impl.poll.stop();
        if (impl.sink) {
            impl.sink->reset();
            impl.sink.reset();
        }
#ifdef HELLOUTAU_AUDIO_PULL_DEVICE
        impl.device.reset();
#endif
        impl.mixer.reset();
        impl.opened = {};
    }

    std::optional<AudioMixer::SourceId> AudioEngine::start(std::shared_ptr<AudioSource> source,
                                                           int rate, QString *error) {
        stdc_impl_t;
        const auto fail = [error](const QString &message) -> std::optional<AudioMixer::SourceId> {
            if (error) {
                *error = message;
            }
            return std::nullopt;
        };
        if (!source) {
            return fail(tr("There is no audio source."));
        }
        const auto selected = device();
        if (selected.isNull()) {
            return fail(tr("The selected audio output device is unavailable."));
        }
        auto format = selected.preferredFormat();
        format.setSampleFormat(QAudioFormat::Float);
        if (rate != format.sampleRate()) {
            return fail(tr("The audio sample rate changed. Start playback again."));
        }
        if (!selected.isFormatSupported(format)) {
            return fail(tr("The audio output device does not support floating-point samples."));
        }
        if (!impl.sink) {
            impl.mixer = std::make_unique<AudioMixer>(rate, format.channelCount());
            impl.sink = std::make_unique<QAudioSink>(selected, format);
            impl.opened = selected;
            auto mixer = impl.mixer.get();
#ifdef HELLOUTAU_AUDIO_PULL_DEVICE
            impl.device = std::make_unique<MixerDevice>(*mixer, format.channelCount());
            impl.sink->start(impl.device.get());
#else
            impl.sink->start(
                [mixer](QSpan<float> samples) { mixer->render(samples.data(), samples.size()); });
#endif
            if (impl.sink->error() != QtAudio::NoError) {
                close();
                return fail(tr("The audio output device could not be started."));
            }
            ++impl.generation;
            impl.poll.start();
        }
        const auto id = impl.mixer->add(std::move(source));
        if (!id) {
            return fail(tr("Too many audio sources are playing."));
        }
        return id;
    }

    void AudioEngine::stop(AudioMixer::SourceId id) {
        stdc_impl_t;
        if (impl.mixer) {
            impl.mixer->remove(id);
        }
    }

    bool AudioEngine::isFinished(AudioMixer::SourceId id) const {
        stdc_impl_t;
        return !impl.mixer || impl.mixer->isFinished(id);
    }

    std::shared_ptr<DeviceClock> AudioEngine::clock(AudioMixer::SourceId id) const {
        stdc_impl_t;
        return impl.mixer ? impl.mixer->clock(id) : nullptr;
    }

    int AudioEngine::bufferedMilliseconds() const {
        stdc_impl_t;
        return impl.sink ? int(impl.sink->format().durationForFrames(
                                   qint32(impl.sink->bufferFrameCount())) /
                               1000)
                         : 0;
    }

    quint64 AudioEngine::streamGeneration() const {
        stdc_impl_t;
        return impl.generation;
    }

}
