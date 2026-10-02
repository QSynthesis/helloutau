#ifndef HELLOUTAU_AUDIO_AUDIOOUTPUT_H
#define HELLOUTAU_AUDIO_AUDIOOUTPUT_H

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QString>

#include <helloutau/Audio/HelloUtauAudioGlobal.h>

namespace hello::daw {

    class AudioSource;

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

    /// Controls one source in the shared AudioEngine. Other outputs continue playing when this
    /// handle stops or is destroyed. Control methods run on the application thread.
    class HELLOUTAU_AUDIO_EXPORT AudioOutput : public QObject {
        Q_OBJECT
    public:
        explicit AudioOutput(QObject *parent = nullptr);
        ~AudioOutput() override;

        /// The sample rate of the default device, to which a source must be converted, or 0 if
        /// the system has no output device.
        static int deviceSampleRate();

        static QList<QByteArray> outputDeviceIds();
        static QString outputDeviceDescription(const QByteArray &id);
        static QByteArray outputDeviceId();
        static void setOutputDeviceId(const QByteArray &id);

        /// Replaces this handle's source. A positive \a sampleRate validates the source rate
        /// against the current device, rejecting data prepared before a device rate change.
        ///
        /// \return whether the device started, with the reason in \a error otherwise
        bool start(std::shared_ptr<AudioSource> source, QString *error = nullptr,
                   int sampleRate = 0);

        /// Stops playing. Emits finished() if something played.
        void stop();

        bool isPlaying() const;

        /// The AudioSource::position() of the source that the device plays now, see
        /// DeviceClock, or none before the device has pulled from it.
        std::optional<double> heardPosition() const;

    Q_SIGNALS:
        /// Playing ended, at the end of the source or on stop().
        void finished();
        /// Reports device loss or an output error after stopping this source.
        void failed(const QString &reason);

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
