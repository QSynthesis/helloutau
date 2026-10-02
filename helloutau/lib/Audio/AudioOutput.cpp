#include "AudioOutput.h"
#include "AudioEngine.h"

#include <algorithm>
#include <atomic>
#include <cmath>

#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtMultimedia/QMediaDevices>
#include <QtMultimedia/QAudioDevice>

#include <stdcorelib/pimpl.h>

#include <r8brain-free-src/CDSPResampler.h>

namespace hello::daw {

    namespace {

        // The block in which the resampler takes its input, in samples
        constexpr int ResamplerBlock = 4096;

        // How often the end of a source is looked for, in milliseconds
        constexpr int PollInterval = 20;

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
        impl.poll.setInterval(PollInterval);
        impl.drain.setSingleShot(true);
        connect(&impl.drain, &QTimer::timeout, this, &AudioOutput::stop);
        connect(&impl.poll, &QTimer::timeout, this, [this] {
            stdc_impl_t;
            if (impl.id && impl.engine && impl.engine->isFinished(*impl.id)) {
                impl.poll.stop();
                impl.drain.start(impl.engine->bufferedMilliseconds() + PollInterval);
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

    int AudioOutput::deviceSampleRate() {
        return AudioEngine::instance()->sampleRate();
    }

    QList<QByteArray> AudioOutput::outputDeviceIds() {
        QList<QByteArray> ids;
        for (const auto &device : QMediaDevices::audioOutputs()) {
            ids.push_back(device.id());
        }
        return ids;
    }

    QString AudioOutput::outputDeviceDescription(const QByteArray &id) {
        for (const auto &device : QMediaDevices::audioOutputs()) {
            if (device.id() == id) {
                return device.description();
            }
        }
        return {};
    }

    QByteArray AudioOutput::outputDeviceId() {
        return AudioEngine::instance()->deviceId();
    }

    void AudioOutput::setOutputDeviceId(const QByteArray &id) {
        AudioEngine::instance()->setDeviceId(id);
    }

    bool AudioOutput::start(std::shared_ptr<AudioSource> source, QString *error, int sampleRate) {
        stdc_impl_t;
        stop();
        impl.id = impl.engine->start(std::move(source),
                                     sampleRate > 0 ? sampleRate : deviceSampleRate(), error);
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
    DeviceClock::DeviceClock(int sampleRate) : m_sampleRate(std::max(1, sampleRate)) {
    }

    void DeviceClock::pulled(qsizetype frames, double before, double after,
                             Clock::time_point now) noexcept {
        const qint64 count = m_count.load(std::memory_order_relaxed);
        if (count == 0) {
            m_start.store(now.time_since_epoch().count(), std::memory_order_relaxed);
            m_initial.store(before, std::memory_order_relaxed);
        }
        m_frames += frames;
        auto &pull = m_pulls[size_t(count % Kept)];
        pull.end.store(m_frames, std::memory_order_relaxed);
        pull.after.store(after, std::memory_order_relaxed);
        m_count.store(count + 1, std::memory_order_release);
    }

    std::optional<double> DeviceClock::heard(Clock::time_point now) const {
        const qint64 count = m_count.load(std::memory_order_acquire);
        if (count == 0) {
            return std::nullopt;
        }
        const Clock::time_point start{Clock::duration(m_start.load(std::memory_order_relaxed))};
        // The frame played now, counted from the first frame pulled
        const double played =
            std::chrono::duration<double>(now - start).count() * double(m_sampleRate);
        // Newest first, short of the oldest pulls, which the audio thread may be replacing
        const qint64 oldest = std::max<qint64>(0, count - (Kept - 2));
        for (qint64 i = count - 1; i >= oldest; --i) {
            const auto &pull = m_pulls[size_t(i % Kept)];
            const auto end = double(pull.end.load(std::memory_order_relaxed));
            const auto after = pull.after.load(std::memory_order_relaxed);
            if (played >= end) {
                return after;
            }
            const auto &previous = m_pulls[size_t((i + Kept - 1) % Kept)];
            const double begin = i == 0 ? 0 : double(previous.end.load(std::memory_order_relaxed));
            const double before = i == 0 ? m_initial.load(std::memory_order_relaxed)
                                         : previous.after.load(std::memory_order_relaxed);
            if (played >= begin || i == oldest) {
                const double part =
                    end > begin ? std::clamp((played - begin) / (end - begin), 0.0, 1.0) : 1.0;
                return before + part * (after - before);
            }
        }
        return std::nullopt;
    }

    std::vector<float> resampled(const std::vector<float> &samples, int channels, int sourceRate,
                                 int targetRate) {
        if (sourceRate == targetRate || channels < 1 || samples.empty()) {
            return samples;
        }
        const qsizetype frames = qsizetype(samples.size()) / channels;
        const auto outFrames = qsizetype(std::llround(double(frames) * targetRate / sourceRate));
        std::vector<float> result(size_t(outFrames * channels));

        std::vector<float> input(static_cast<size_t>(frames));
        std::vector<float> output(static_cast<size_t>(outFrames));
        r8b::CDSPResampler24 resampler(sourceRate, targetRate, ResamplerBlock);
        for (int channel = 0; channel < channels; ++channel) {
            for (qsizetype i = 0; i < frames; ++i) {
                input[size_t(i)] = samples[size_t(i * channels + channel)];
            }
            resampler.oneshot(input.data(), int(frames), output.data(), int(outFrames));
            for (qsizetype i = 0; i < outFrames; ++i) {
                result[size_t(i * channels + channel)] = output[size_t(i)];
            }
        }
        return result;
    }

}
