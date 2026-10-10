#ifndef HELLOUTAU_AUDIO_PIANOTONESOURCE_H
#define HELLOUTAU_AUDIO_PIANOTONESOURCE_H

#include <array>
#include <atomic>

#include <helloutau/Audio/AudioSource.h>

namespace hello::daw {

    /// A short piano-like tone made from damped inharmonic modes and a hammer transient.
    ///
    /// The tone is synthesized without a piano sample, for the piano keyboard rather than for
    /// rendering.
    class HELLOUTAU_AUDIO_EXPORT PianoToneSource : public AudioSource {
    public:
        PianoToneSource(int sampleRate, double frequency, double duration = 0.8,
                        double amplitude = 0.22);
        ~PianoToneSource() override;

        qsizetype read(float *out, qsizetype frames, int channels) noexcept override;
        qint64 position() const override;

    private:
        int m_sampleRate;
        double m_amplitude;
        qsizetype m_frames;
        std::array<double, 10> m_angularFrequencies;
        std::atomic<qsizetype> m_position = 0;
    };

}

#endif // HELLOUTAU_AUDIO_PIANOTONESOURCE_H
