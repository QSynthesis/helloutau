#ifndef HELLOUTAU_AUDIO_STREAMSOURCE_H
#define HELLOUTAU_AUDIO_STREAMSOURCE_H

#include <functional>
#include <memory>

#include <helloutau/Audio/AudioSource.h>

namespace hello::daw {

    /// Audio produced while it plays: mono samples pulled from a generator on a thread of the
    /// source, converted to the rate of the device there, and passed to the audio thread through
    /// a ring buffer that neither locks nor allocates.
    ///
    /// When the generator cannot keep up, as when a note is not yet rendered, the device plays
    /// silence and position() stands still until samples arrive again: playback waits rather than
    /// skips. See the section on realtime rendering in docs/Synth.md.
    class HELLOUTAU_AUDIO_EXPORT StreamSource : public AudioSource {
    public:
        using Generator = std::function<qsizetype(float *out, qsizetype frames)>;

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

#endif
