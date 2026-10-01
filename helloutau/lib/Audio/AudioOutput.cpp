#include "AudioOutput.h"

#include <algorithm>
#include <atomic>
#include <cmath>

#include <QtCore/QTimer>
#include <QtMultimedia/QAudioSink>
#include <QtMultimedia/QMediaDevices>

#include <stdcorelib/pimpl.h>

#include <r8brain-free-src/CDSPResampler.h>

namespace hello::daw {

    namespace {
        QByteArray PreferredOutputDevice;
    }

    namespace {

        // The block in which the resampler takes its input, in samples
        constexpr int ResamplerBlock = 4096;

        // How often the end of a source is looked for, in milliseconds
        constexpr int PollInterval = 20;

    }

    AudioSource::~AudioSource() = default;

    class BufferSource::Impl {
    public:
        using Decl = BufferSource;

        std::shared_ptr<const std::vector<float>> samples;
        int channels = 1;
        std::atomic<qsizetype> position = 0;
    };

    BufferSource::BufferSource(std::vector<float> samples, int channels)
        : BufferSource(std::make_shared<const std::vector<float>>(std::move(samples)), channels) {
    }

    BufferSource::BufferSource(std::shared_ptr<const std::vector<float>> samples, int channels,
                               qsizetype first)
        : _impl(std::make_unique<Impl>()) {
        stdc_impl_t;
        impl.samples = samples ? std::move(samples) : std::make_shared<const std::vector<float>>();
        impl.channels = std::max(1, channels);
        impl.position = std::clamp<qsizetype>(first, 0, frameCount());
    }

    BufferSource::~BufferSource() = default;

    qsizetype BufferSource::read(float *out, qsizetype frames, int channels) noexcept {
        stdc_impl_t;
        const int own = impl.channels;
        const auto position = impl.position.load(std::memory_order_relaxed);
        const auto count = std::clamp<qsizetype>(frameCount() - position, 0, frames);
        const float *from = impl.samples->data() + position * own;
        for (qsizetype frame = 0; frame < count; ++frame) {
            for (int channel = 0; channel < channels; ++channel) {
                // Mono on every channel; otherwise channel by channel, silence beyond them
                const int source = own == 1 ? 0 : channel;
                *out++ = source < own ? from[source] : 0.0f;
            }
            from += own;
        }
        impl.position.store(position + count, std::memory_order_relaxed);
        return count;
    }

    qsizetype BufferSource::frameCount() const {
        stdc_impl_t;
        return qsizetype(impl.samples->size()) / impl.channels;
    }

    qint64 BufferSource::position() const {
        stdc_impl_t;
        return impl.position.load(std::memory_order_relaxed);
    }

    namespace {

        // Writes the samples of \a source into \a out, \a channels interleaved, silence after its
        // end, which \a ended then reports, and records the pull in \a clock. Called on the
        // thread that the device pulls on.
        void fill(AudioSource &source, DeviceClock &clock, float *out, qsizetype frames,
                  int channels, std::atomic<bool> &ended) {
            const auto now = DeviceClock::Clock::now();
            const auto before = double(source.position());
            const auto written = source.read(out, frames, channels);
            if (written < frames) {
                std::fill(out + written * channels, out + frames * channels, 0.0f);
                ended.store(true);
            }
            clock.pulled(frames, before, double(source.position()), now);
        }

#if QT_VERSION < QT_VERSION_CHECK(6, 11, 0) || defined(HELLOUTAU_AUDIO_PULL)
#  define HELLOUTAU_AUDIO_PULL_DEVICE
        // The device that QAudioSink pulls from before Qt 6.11, which added the callback
        // interface; macOS builds use Qt 6.10. The device reads the source in whole frames of
        // 32-bit floating-point samples.
        class SourceDevice : public QIODevice {
        public:
            SourceDevice(std::shared_ptr<AudioSource> source, std::shared_ptr<DeviceClock> clock,
                         int channels, std::shared_ptr<std::atomic<bool>> ended)
                : m_source(std::move(source)), m_clock(std::move(clock)), m_channels(channels),
                  m_ended(std::move(ended)) {
            }

            bool isSequential() const override {
                return true;
            }

            // Always ready: after its end the source reads as silence.
            qint64 bytesAvailable() const override {
                return QIODevice::bytesAvailable() + std::numeric_limits<qint32>::max();
            }

        protected:
            qint64 readData(char *data, qint64 maxSize) override {
                const qint64 frameBytes = qint64(sizeof(float)) * m_channels;
                const qint64 frames = maxSize / frameBytes;
                fill(*m_source, *m_clock, reinterpret_cast<float *>(data), frames, m_channels,
                     *m_ended);
                return frames * frameBytes;
            }

            qint64 writeData(const char *data, qint64 size) override {
                Q_UNUSED(data);
                Q_UNUSED(size);
                return -1;
            }

        private:
            std::shared_ptr<AudioSource> m_source;
            std::shared_ptr<DeviceClock> m_clock;
            int m_channels;
            std::shared_ptr<std::atomic<bool>> m_ended;
        };
#endif

    }

    class AudioOutput::Impl {
    public:
        using Decl = AudioOutput;

        explicit Impl(Decl *decl) : _decl(decl) {
        }

        Decl *_decl;
        std::unique_ptr<QAudioSink> sink;
#ifdef HELLOUTAU_AUDIO_PULL_DEVICE
        std::unique_ptr<QIODevice> device;
#endif
        // Set on the audio thread once the source has ended
        std::shared_ptr<std::atomic<bool>> ended;
        // Written on the audio thread
        std::shared_ptr<DeviceClock> clock;
        QTimer poll;
        bool draining = false;
    };

    AudioOutput::AudioOutput(QObject *parent)
        : QObject(parent), _impl(std::make_unique<Impl>(this)) {
        stdc_impl_t;
        impl.poll.setInterval(PollInterval);
        connect(&impl.poll, &QTimer::timeout, this, [this] {
            stdc_impl_t;
            if (!impl.ended || !impl.ended->load() || impl.draining) {
                return;
            }
            // What the device has buffered still plays after the source has ended.
            impl.draining = true;
            const auto format = impl.sink->format();
            const int buffered =
                int(format.durationForFrames(qint32(impl.sink->bufferFrameCount())) / 1000);
            QTimer::singleShot(buffered + PollInterval, this, [this] {
                stdc_impl_t;
                if (impl.draining) {
                    stop();
                }
            });
        });
    }

    AudioOutput::~AudioOutput() {
        stdc_impl_t;
        if (impl.sink) {
            impl.sink->stop();
        }
    }

    int AudioOutput::deviceSampleRate() {
        auto device = QMediaDevices::defaultAudioOutput();
        if (!PreferredOutputDevice.isEmpty()) {
            for (const auto &candidate : QMediaDevices::audioOutputs()) {
                if (candidate.id() == PreferredOutputDevice) {
                    device = candidate;
                    break;
                }
            }
        }
        return device.isNull() ? 0 : device.preferredFormat().sampleRate();
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
        return PreferredOutputDevice;
    }

    void AudioOutput::setOutputDeviceId(const QByteArray &id) {
        PreferredOutputDevice = id;
    }

    bool AudioOutput::start(std::shared_ptr<AudioSource> source, QString *error) {
        stdc_impl_t;
        stop();
        const auto fail = [error](const QString &message) {
            if (error) {
                *error = message;
            }
            return false;
        };

        auto device = QMediaDevices::defaultAudioOutput();
        if (!PreferredOutputDevice.isEmpty()) {
            for (const auto &candidate : QMediaDevices::audioOutputs()) {
                if (candidate.id() == PreferredOutputDevice) {
                    device = candidate;
                    break;
                }
            }
        }
        if (device.isNull()) {
            return fail(tr("There is no audio output device."));
        }
        auto format = device.preferredFormat();
        format.setSampleFormat(QAudioFormat::Float);
        if (!device.isFormatSupported(format)) {
            return fail(tr("The audio output device \"%1\" does not accept floating-point samples.")
                            .arg(device.description()));
        }

        impl.sink = std::make_unique<QAudioSink>(device, format);
        auto ended = std::make_shared<std::atomic<bool>>(false);
        impl.ended = ended;
        auto clock = std::make_shared<DeviceClock>(format.sampleRate());
        impl.clock = clock;
        impl.draining = false;
        const int channels = format.channelCount();
#ifdef HELLOUTAU_AUDIO_PULL_DEVICE
        impl.device =
            std::make_unique<SourceDevice>(std::move(source), std::move(clock), channels, ended);
        impl.device->open(QIODevice::ReadOnly);
        impl.sink->start(impl.device.get());
#else
        impl.sink->start([source = std::move(source), clock = std::move(clock), ended,
                          channels](QSpan<float> buffer) {
            fill(*source, *clock, buffer.data(), buffer.size() / channels, channels, *ended);
        });
#endif
        if (impl.sink->error() != QtAudio::NoError) {
            impl.sink.reset();
            return fail(tr("The audio output device \"%1\" could not be started.")
                            .arg(device.description()));
        }
        impl.poll.start();
        return true;
    }

    void AudioOutput::stop() {
        stdc_impl_t;
        if (!impl.sink) {
            return;
        }
        impl.poll.stop();
        impl.draining = false;
        impl.sink->stop();
        impl.sink.reset();
#ifdef HELLOUTAU_AUDIO_PULL_DEVICE
        impl.device.reset();
#endif
        impl.ended.reset();
        impl.clock.reset();
        Q_EMIT finished();
    }

    bool AudioOutput::isPlaying() const {
        stdc_impl_t;
        return bool(impl.sink);
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
