#ifndef HELLOUTAU_AUDIO_DEVICECLOCK_H
#define HELLOUTAU_AUDIO_DEVICECLOCK_H

#include <array>
#include <atomic>
#include <chrono>
#include <optional>

#include <helloutau/Audio/HelloUtauAudioGlobal.h>

namespace hello::daw {

    /// The position of a source that a device plays at a given moment, computed from the pulls of
    /// the device: the frames of each pull and the AudioSource::position() after it.
    ///
    /// A device pulls frames ahead of playing them by the length of its buffers, 41 to 50 ms with
    /// WASAPI (measured, see the section on audio output in docs/Widgets.md). The first pulled
    /// frame is taken to play at the time of the first pull, and every later frame at the sample
    /// rate. The position at a moment is therefore interpolated within the pull that holds the
    /// frame played at that moment. While the source is starved, its position is constant across
    /// the pull, and so is the result.
    ///
    /// The first pull of a source is the first pull of the stream only if the source opened the
    /// stream, which AudioEngine closes when no source plays. A source that joins a playing
    /// stream is heard later than the clock reports, by up to the buffers of the device. A clock
    /// that counted from the opening of the stream instead would drift from the device over a
    /// long session, because the stream would stay open.
    ///
    /// The audio thread writes the clock without locking or allocating, and any other thread
    /// reads it. The last keptPulls pulls are kept, which span many times the buffers of a device.
    class HELLOUTAU_AUDIO_EXPORT DeviceClock {
    public:
        using Clock = std::chrono::steady_clock;

        explicit DeviceClock(int sampleRate);

        /// Records a pull of \a frames at \a now, the source at \a before before it and at
        /// \a after after it. The first pull starts the clock.
        void pulled(qsizetype frames, double before, double after, Clock::time_point now) noexcept;

        /// Returns the position of the source that the device plays at \a now, or
        /// \c std::nullopt before the first pull.
        std::optional<double> heard(Clock::time_point now) const;

    private:
        static constexpr int keptPulls = 64;

        struct Pull {
            std::atomic<qint64> end{0};
            std::atomic<double> after{0};
        };

        int m_sampleRate;
        std::array<Pull, keptPulls> m_pulls;
        std::atomic<qint64> m_count{0};
        std::atomic<Clock::rep> m_start{0};
        std::atomic<double> m_initial{0};
        qint64 m_frames = 0;
    };

}

#endif // HELLOUTAU_AUDIO_DEVICECLOCK_H
