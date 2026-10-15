#ifndef HELLOKIT_AUDIOFILE_SPECTROGRAM_H
#define HELLOKIT_AUDIOFILE_SPECTROGRAM_H

#include <functional>
#include <optional>
#include <vector>

#include <hellokit/AudioFile/HelloKitAudioFileGlobal.h>
#include <hellokit/AudioFile/WaveAudio.h>

namespace hello::kit {

    /// The magnitude spectrum of audio over time, for display. See docs/FrequencyMaps.md.
    ///
    /// A frame is a Hamming window of windowSize samples centered every hopSize samples, the
    /// channels mixed, the audio taken as silent beyond its ends. Its spectrum has
    /// windowSize / 2 + 1 bins from 0 to half the sample rate. The sizes are those of the
    /// spectrum of frqeditor, which users of frequency maps know.
    class HELLOKIT_AUDIOFILE_EXPORT Spectrogram {
    public:
        static constexpr int windowSize = 2048;
        static constexpr int hopSize = 512;
        static constexpr int binCount = windowSize / 2 + 1;

        Spectrogram();

        /// Computes the spectrogram of \a audio, which is empty without samples.
        static Spectrogram of(const WaveAudio &audio);

        /// \overload that calls \a canceled before every 16 frames, from the calling thread, and
        /// stops if it returns true.
        ///
        /// \return the spectrogram, or \c std::nullopt if canceled
        static std::optional<Spectrogram> of(const WaveAudio &audio,
                                             const std::function<bool()> &canceled);

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
        // Computes the spectrogram of audio and stops if canceled() returns true, which it calls
        // before every 16 frames. Defined and instantiated in the source only.
        template <class Canceled>
        static std::optional<Spectrogram> compute(const WaveAudio &audio, const Canceled &canceled);

        int m_sampleRate = 0;
        int m_frameCount = 0;
        float m_peak = 0;
        std::vector<float> m_magnitudes;
    };

}

#endif // HELLOKIT_AUDIOFILE_SPECTROGRAM_H
