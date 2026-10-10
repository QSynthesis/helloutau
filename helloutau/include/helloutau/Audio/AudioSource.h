#ifndef HELLOUTAU_AUDIO_AUDIOSOURCE_H
#define HELLOUTAU_AUDIO_AUDIOSOURCE_H

#include <helloutau/Audio/HelloUtauAudioGlobal.h>

namespace hello::daw {

    /// A source of samples, which AudioMixer reads on the audio thread as the device requires
    /// them.
    ///
    /// \warning read() and position() are called on the audio thread of the device, and must
    ///          neither block, lock nor allocate, as the Qt documentation of the callback
    ///          interface of \c QAudioSink requires.
    class HELLOUTAU_AUDIO_EXPORT AudioSource {
    public:
        virtual ~AudioSource();

        /// Writes up to \a frames frames of \a channels interleaved samples to \a out, and
        /// returns the number written. Fewer than requested marks the end of the source.
        virtual qsizetype read(float *out, qsizetype frames, int channels) noexcept = 0;

        /// Returns how far the source has been read, in its own frames. Any thread may call it.
        virtual qint64 position() const noexcept = 0;
    };

}

#endif // HELLOUTAU_AUDIO_AUDIOSOURCE_H
