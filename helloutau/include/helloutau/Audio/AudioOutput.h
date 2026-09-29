#ifndef HELLOUTAU_AUDIO_AUDIOOUTPUT_H
#define HELLOUTAU_AUDIO_AUDIOOUTPUT_H

#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QString>

#include <helloutau/Audio/HelloUtauAudioGlobal.h>

namespace hello::daw {

    /// Samples that an AudioOutput plays, pulled as the device needs them.
    ///
    /// \warning read() is called on the audio thread of the device, and must neither block,
    ///          lock nor allocate, as the Qt documentation of the callback interface of
    ///          \c QAudioSink requires.
    class HELLOUTAU_AUDIO_EXPORT AudioSource {
    public:
        virtual ~AudioSource();

        /// Writes up to \a frames frames of \a channels interleaved samples to \a out, and
        /// returns the number written. Fewer than requested marks the end of the source.
        virtual qsizetype read(float *out, qsizetype frames, int channels) noexcept = 0;

        /// How far the source has been read, in its own frames, which any thread may query.
        virtual qint64 position() const = 0;
    };

    /// Audio held in memory at the rate of the device, played from its start to its end.
    ///
    /// A mono buffer is played on every channel; a buffer of more channels than the device has
    /// plays its first ones.
    class HELLOUTAU_AUDIO_EXPORT BufferSource : public AudioSource {
    public:
        /// \a samples interleaved by \a channels.
        BufferSource(std::vector<float> samples, int channels);

        /// \a samples interleaved by \a channels, shared, played from frame \a first on: to
        /// play a render again, or on from where it was paused.
        BufferSource(std::shared_ptr<const std::vector<float>> samples, int channels,
                     qsizetype first = 0);
        ~BufferSource() override;

        qsizetype read(float *out, qsizetype frames, int channels) noexcept override;

        qsizetype frameCount() const;

        /// The frame to be read next, which starts at the first played.
        qint64 position() const override;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

    /// Audio produced while it plays: mono samples pulled from a generator on a thread of the
    /// source, converted to the rate of the device there, and passed to the audio thread through
    /// a ring buffer that neither locks nor allocates.
    ///
    /// When the generator cannot keep up, as when a note is not yet rendered, the device plays
    /// silence and position() stands still until samples arrive again: playback waits rather than
    /// skips. See the section on realtime rendering in docs/Synth.md.
    class HELLOUTAU_AUDIO_EXPORT StreamSource : public AudioSource {
    public:
        /// Writes up to \a frames samples to \a out and returns how many: a positive number, 0
        /// if none are available yet, or a negative number at the end. May block briefly.
        using Generator = std::function<qsizetype(float *out, qsizetype frames)>;

        /// \param buffer the seconds of audio produced ahead of the device
        StreamSource(Generator generator, int sourceRate, int deviceRate, double buffer = 0.5);
        ~StreamSource() override;

        /// Starts the thread that produces the audio.
        void start();

        /// Ends the thread that produces the audio and waits for it, so that the generator is
        /// not called any more, even while the device still holds the source. Reading gives
        /// what was produced, and silence after it.
        void stop();

        qsizetype read(float *out, qsizetype frames, int channels) noexcept override;

        /// The samples of the generator read by the device so far, at its rate.
        qint64 position() const override;

        /// Whether the device last found no samples before the end, and plays silence meanwhile.
        bool isStarved() const;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

    /// Finds what a device plays at a moment from what it has pulled: the frames of each pull and
    /// the AudioSource::position() after it.
    ///
    /// A device pulls ahead of what it plays by what its buffers hold, some 45 ms with WASAPI
    /// (measured, see the section on audio output in docs/Widgets.md), and plays as time passes
    /// from its first pull. The position that
    /// the device plays at a moment is therefore the position after the pull that holds the
    /// frame played then, found as far into the pull as that frame is. While the source is
    /// starved, its position stands still across the pull, and so does the one found.
    ///
    /// Written on the audio thread, which neither locks nor allocates here, and read on any
    /// other. The last pulls are kept, far more than a device holds.
    class HELLOUTAU_AUDIO_EXPORT DeviceClock {
    public:
        using Clock = std::chrono::steady_clock;

        explicit DeviceClock(int sampleRate);

        /// Records a pull of \a frames at \a now, the source at \a before before it and at
        /// \a after after it. The first pull starts the clock.
        void pulled(qsizetype frames, double before, double after, Clock::time_point now) noexcept;

        /// The position of the source that the device plays at \a now, or none before the first
        /// pull.
        std::optional<double> heard(Clock::time_point now) const;

    private:
        static constexpr int Kept = 64;

        struct Pull {
            std::atomic<qint64> end{0};
            std::atomic<double> after{0};
        };

        int m_sampleRate;
        std::array<Pull, Kept> m_pulls;
        std::atomic<qint64> m_count{0};
        std::atomic<Clock::rep> m_start{0};
        std::atomic<double> m_initial{0};
        qint64 m_frames = 0;
    };

    /// Plays an AudioSource on the default output device of the system.
    ///
    /// Wraps the callback interface of \c QAudioSink (Qt 6.11), so that the device output can be
    /// replaced without affecting its users. See the section on audio output in
    /// docs/Widgets.md.
    class HELLOUTAU_AUDIO_EXPORT AudioOutput : public QObject {
        Q_OBJECT
    public:
        explicit AudioOutput(QObject *parent = nullptr);
        ~AudioOutput() override;

        /// The sample rate of the default device, to which a source must be converted, or 0 if
        /// the system has no output device.
        static int deviceSampleRate();

        /// Starts playing \a source on the default device, stopping what played before.
        ///
        /// \return whether the device started, with the reason in \a error otherwise
        bool start(std::shared_ptr<AudioSource> source, QString *error = nullptr);

        /// Stops playing. Emits finished() if something played.
        void stop();

        bool isPlaying() const;

        /// The AudioSource::position() of the source that the device plays now, see
        /// DeviceClock, or none before the device has pulled from it.
        std::optional<double> heardPosition() const;

    Q_SIGNALS:
        /// Playing ended, at the end of the source or on stop().
        void finished();

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

    /// Converts \a samples, interleaved by \a channels, from \a sourceRate to \a targetRate with
    /// r8brain-free-src, each channel separately. The result has the same duration, rounded to
    /// whole frames.
    HELLOUTAU_AUDIO_EXPORT std::vector<float>
        resampled(const std::vector<float> &samples, int channels, int sourceRate, int targetRate);

}

#endif // HELLOUTAU_AUDIO_AUDIOOUTPUT_H
