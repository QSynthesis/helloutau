#ifndef HELLOUTAU_AUDIO_SINEWAVESOURCE_H
#define HELLOUTAU_AUDIO_SINEWAVESOURCE_H

#include <atomic>

#include <helloutau/Audio/AudioSource.h>

namespace hello::daw {

    /// A finite mono sine wave with short linear fades at both ends.
    class HELLOUTAU_AUDIO_EXPORT SineWaveSource : public AudioSource {
    public:
        SineWaveSource(int sampleRate, double frequency, double duration, double amplitude = 0.18);
        ~SineWaveSource() override;

        qsizetype read(float *out, qsizetype frames, int channels) noexcept override;
        qint64 position() const noexcept override;

    private:
        int m_sampleRate;
        double m_angularFrequency;
        double m_amplitude;
        qsizetype m_frames;
        std::atomic<qsizetype> m_position = 0;
    };

}

#endif // HELLOUTAU_AUDIO_SINEWAVESOURCE_H
