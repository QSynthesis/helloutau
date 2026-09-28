#ifndef HELLOUTAU_AUDIO_AUDIOOUTPUT_H
#define HELLOUTAU_AUDIO_AUDIOOUTPUT_H

#include <memory>
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
    };

    /// Audio held in memory at the rate of the device, played from its start to its end.
    ///
    /// A mono buffer is played on every channel; a buffer of more channels than the device has
    /// plays its first ones.
    class HELLOUTAU_AUDIO_EXPORT BufferSource : public AudioSource {
    public:
        /// \a samples interleaved by \a channels.
        BufferSource(std::vector<float> samples, int channels);
        ~BufferSource() override;

        qsizetype read(float *out, qsizetype frames, int channels) noexcept override;

        qsizetype frameCount() const;

        /// The frames read so far, which any thread may query.
        qsizetype position() const;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
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

        /// The time the device has played since start(), in milliseconds.
        double elapsed() const;

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
