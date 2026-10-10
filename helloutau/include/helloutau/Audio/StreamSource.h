#ifndef HELLOUTAU_AUDIO_STREAMSOURCE_H
#define HELLOUTAU_AUDIO_STREAMSOURCE_H

#include <functional>
#include <memory>

#include <helloutau/Audio/AudioSource.h>

namespace hello::daw {

    /// Audio produced while it plays. A thread of the source pulls mono samples from a generator,
    /// converts them to the rate of the device, and passes them to the audio thread through a ring
    /// buffer that neither locks nor allocates.
    ///
    /// If the generator falls behind, for example while a note is not yet rendered, the device
    /// plays silence and position() does not advance until samples arrive. Playback therefore
    /// waits instead of skipping. See the section on realtime rendering in docs/Synth.md.
    class HELLOUTAU_AUDIO_EXPORT StreamSource : public AudioSource {
    public:
        using Generator = std::function<qsizetype(float *out, qsizetype frames)>;

        /// Constructs a source of the samples of \a generator, which are at \a sourceRate, for a
        /// device at \a deviceRate. \a buffer is the length of the ring buffer in seconds.
        StreamSource(Generator generator, int sourceRate, int deviceRate, double buffer = 0.5);
        ~StreamSource() override;

        void start();
        void stop();
        qsizetype read(float *out, qsizetype frames, int channels) noexcept override;
        qint64 position() const noexcept override;
        bool isStarved() const;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_AUDIO_STREAMSOURCE_H
