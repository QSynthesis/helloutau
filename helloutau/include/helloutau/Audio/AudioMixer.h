#ifndef HELLOUTAU_AUDIO_AUDIOMIXER_H
#define HELLOUTAU_AUDIO_AUDIOMIXER_H

#include <memory>
#include <optional>

#include <helloutau/Audio/AudioSource.h>
#include <helloutau/Audio/DeviceClock.h>

namespace hello::daw {

    /// Mixes independent sources without allocating or locking in render(). All other methods
    /// run on one control thread. Destruction requires the render thread to have stopped.
    class HELLOUTAU_AUDIO_EXPORT AudioMixer {
    public:
        using SourceId = quint64;
        static constexpr int capacity = 64;

        AudioMixer(int sampleRate, int channels);
        ~AudioMixer();

        /// Registers a source at the mixer sample rate. Returns no id if full or source is null.
        /// The id is unique in the process, so that an id of a closed mixer never names a source
        /// of a later mixer.
        std::optional<SourceId> add(std::shared_ptr<AudioSource> source);
        /// Removes only this source. An in-progress read may finish before removal takes effect.
        void remove(SourceId id);
        bool isFinished(SourceId id) const;

        /// Returns whether no source is playing.
        bool isIdle() const;
        std::shared_ptr<DeviceClock> clock(SourceId id) const;
        /// Releases completed sources on the control thread.
        void collect();
        /// Fills interleaved samples, including zero padding for an incomplete final frame.
        void render(float *out, qsizetype samples) noexcept;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif
