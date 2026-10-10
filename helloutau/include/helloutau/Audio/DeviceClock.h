#ifndef HELLOUTAU_AUDIO_DEVICECLOCK_H
#define HELLOUTAU_AUDIO_DEVICECLOCK_H

#include <array>
#include <atomic>
#include <chrono>
#include <optional>

#include <helloutau/Audio/HelloUtauAudioGlobal.h>

namespace hello::daw {

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

}

#endif // HELLOUTAU_AUDIO_DEVICECLOCK_H
