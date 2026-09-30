#ifndef HELLOKIT_SYNTH_SPECTROGRAM_H
#define HELLOKIT_SYNTH_SPECTROGRAM_H

#include <vector>

#include <hellokit/Synth/HelloKitSynthGlobal.h>
#include <hellokit/Synth/WaveAudio.h>

namespace hello::kit {

    /// The magnitude spectrum of audio over time, for display. See docs/FrequencyTables.md.
    ///
    /// A frame is a Hamming window of windowSize samples centered every hopSize samples, the
    /// channels mixed, the audio taken as silent beyond its ends; its spectrum has
    /// windowSize / 2 + 1 bins from 0 to half the sample rate. The sizes are those of the
    /// spectrum of frqeditor, which users of frequency tables know.
    class HELLOKIT_SYNTH_EXPORT Spectrogram {
    public:
        static constexpr int windowSize = 2048;
        static constexpr int hopSize = 512;
        static constexpr int binCount = windowSize / 2 + 1;

        Spectrogram();

        /// Computes the spectrogram of \a audio, which is empty without samples.
        static Spectrogram of(const WaveAudio &audio);

        int sampleRate() const;
        int frameCount() const;

        /// The milliseconds of the center of \a frame.
        double timeOf(int frame) const;

        /// The hertz of \a bin, which may be fractional.
        double frequencyOf(double bin) const;

        /// The bin of \a frequency in hertz, the inverse of frequencyOf().
        double binOf(double frequency) const;

        /// The amplitude of a sine at \a bin in \a frame, from 0 for silence to 1 for a sine of
        /// full scale.
        float magnitude(int frame, int bin) const;

        /// The largest magnitude, or 0 if empty.
        float peak() const;

    private:
        int m_sampleRate = 0;
        int m_frameCount = 0;
        float m_peak = 0;
        std::vector<float> m_magnitudes;
    };

}

#endif // HELLOKIT_SYNTH_SPECTROGRAM_H
