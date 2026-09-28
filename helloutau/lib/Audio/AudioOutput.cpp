#include "AudioOutput.h"

#include <algorithm>
#include <atomic>
#include <cmath>

#include <QtCore/QTimer>
#include <QtMultimedia/QAudioSink>
#include <QtMultimedia/QMediaDevices>

#include <r8brain-free-src/CDSPResampler.h>

namespace hello::daw {

    namespace {

        // The block in which the resampler takes its input, in samples
        constexpr int ResamplerBlock = 4096;

        // How often the end of a source is looked for, in milliseconds
        constexpr int PollInterval = 20;

    }

    AudioSource::~AudioSource() = default;

    class BufferSource::Impl {
    public:
        std::vector<float> samples;
        int channels = 1;
        std::atomic<qsizetype> position = 0;
    };

    BufferSource::BufferSource(std::vector<float> samples, int channels)
        : _impl(std::make_unique<Impl>()) {
        _impl->samples = std::move(samples);
        _impl->channels = std::max(1, channels);
    }

    BufferSource::~BufferSource() = default;

    qsizetype BufferSource::read(float *out, qsizetype frames, int channels) noexcept {
        const int own = _impl->channels;
        const auto position = _impl->position.load(std::memory_order_relaxed);
        const auto count = std::clamp<qsizetype>(frameCount() - position, 0, frames);
        const float *from = _impl->samples.data() + position * own;
        for (qsizetype frame = 0; frame < count; ++frame) {
            for (int channel = 0; channel < channels; ++channel) {
                // Mono on every channel; otherwise channel by channel, silence beyond them
                const int source = own == 1 ? 0 : channel;
                *out++ = source < own ? from[source] : 0.0f;
            }
            from += own;
        }
        _impl->position.store(position + count, std::memory_order_relaxed);
        return count;
    }

    qsizetype BufferSource::frameCount() const {
        return qsizetype(_impl->samples.size()) / _impl->channels;
    }

    qsizetype BufferSource::position() const {
        return _impl->position.load(std::memory_order_relaxed);
    }

    class AudioOutput::Impl {
    public:
        explicit Impl(AudioOutput *decl) : _decl(decl) {
        }

        AudioOutput *_decl;
        std::unique_ptr<QAudioSink> sink;
        // Set on the audio thread once the source has ended
        std::shared_ptr<std::atomic<bool>> ended;
        QTimer poll;
        bool draining = false;
    };

    AudioOutput::AudioOutput(QObject *parent)
        : QObject(parent), _impl(std::make_unique<Impl>(this)) {
        _impl->poll.setInterval(PollInterval);
        connect(&_impl->poll, &QTimer::timeout, this, [this] {
            if (!_impl->ended || !_impl->ended->load() || _impl->draining) {
                return;
            }
            // What the device has buffered still plays after the source has ended.
            _impl->draining = true;
            const auto format = _impl->sink->format();
            const int buffered =
                int(format.durationForFrames(qint32(_impl->sink->bufferFrameCount())) / 1000);
            QTimer::singleShot(buffered + PollInterval, this, [this] {
                if (_impl->draining) {
                    stop();
                }
            });
        });
    }

    AudioOutput::~AudioOutput() {
        if (_impl->sink) {
            _impl->sink->stop();
        }
    }

    int AudioOutput::deviceSampleRate() {
        const auto device = QMediaDevices::defaultAudioOutput();
        return device.isNull() ? 0 : device.preferredFormat().sampleRate();
    }

    bool AudioOutput::start(std::shared_ptr<AudioSource> source, QString *error) {
        stop();
        const auto fail = [error](const QString &message) {
            if (error) {
                *error = message;
            }
            return false;
        };

        const auto device = QMediaDevices::defaultAudioOutput();
        if (device.isNull()) {
            return fail(tr("There is no audio output device."));
        }
        auto format = device.preferredFormat();
        format.setSampleFormat(QAudioFormat::Float);
        if (!device.isFormatSupported(format)) {
            return fail(tr("The audio output device \"%1\" does not accept floating-point samples.")
                            .arg(device.description()));
        }

        _impl->sink = std::make_unique<QAudioSink>(device, format);
        auto ended = std::make_shared<std::atomic<bool>>(false);
        _impl->ended = ended;
        _impl->draining = false;
        const int channels = format.channelCount();
        _impl->sink->start([source = std::move(source), ended, channels](QSpan<float> buffer) {
            const qsizetype frames = buffer.size() / channels;
            const auto written = source->read(buffer.data(), frames, channels);
            if (written < frames) {
                std::fill(buffer.begin() + written * channels, buffer.end(), 0.0f);
                ended->store(true);
            }
        });
        if (_impl->sink->error() != QtAudio::NoError) {
            _impl->sink.reset();
            return fail(tr("The audio output device \"%1\" could not be started.")
                            .arg(device.description()));
        }
        _impl->poll.start();
        return true;
    }

    void AudioOutput::stop() {
        if (!_impl->sink) {
            return;
        }
        _impl->poll.stop();
        _impl->draining = false;
        _impl->sink->stop();
        _impl->sink.reset();
        _impl->ended.reset();
        Q_EMIT finished();
    }

    bool AudioOutput::isPlaying() const {
        return bool(_impl->sink);
    }

    double AudioOutput::elapsed() const {
        return _impl->sink ? double(_impl->sink->processedUSecs()) / 1000 : 0;
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
