#ifndef HELLOUTAU_AUDIO_BUFFERSOURCE_H
#define HELLOUTAU_AUDIO_BUFFERSOURCE_H

#include <memory>
#include <vector>

#include <helloutau/Audio/AudioSource.h>

namespace hello::daw {

    /// Audio held in memory at the rate of the device, played from its start to its end.
    ///
    /// A mono buffer is played on every channel. Of a buffer of more channels than the device
    /// has, only the first channels are played.
    class HELLOUTAU_AUDIO_EXPORT BufferSource : public AudioSource {
    public:
        /// Constructs a source of \a samples, interleaved by \a channels.
        BufferSource(std::vector<float> samples, int channels);

        /// Constructs a source of the shared \a samples, interleaved by \a channels, that plays
        /// from frame \a first, clamped to the buffer. Used to play a render again or to resume
        /// it from where it was paused.
        BufferSource(std::shared_ptr<const std::vector<float>> samples, int channels,
                     qsizetype first = 0);
        ~BufferSource() override;

        qsizetype read(float *out, qsizetype frames, int channels) noexcept override;
        qsizetype frameCount() const;
        qint64 position() const noexcept override;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_AUDIO_BUFFERSOURCE_H
