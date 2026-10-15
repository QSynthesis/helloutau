#ifndef HELLOKIT_VOICEBANK_FREQUENCYMAP_H
#define HELLOKIT_VOICEBANK_FREQUENCYMAP_H

#include <cstddef>
#include <optional>
#include <vector>

#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>

namespace hello::kit {

    /// The fundamental frequency of an audio file as a resampler analyzed it, in the terms that
    /// every format shares. See docs/FrequencyMaps.md.
    ///
    /// A format writes these terms and keeps the parts of an existing map beyond them.
    struct HELLOKIT_VOICEBANK_EXPORT FrequencyMap {
        struct Frame {
            /// Milliseconds from the start of the audio file.
            double time = 0;

            /// Hertz, or 0 for a frame that the format marks unvoiced.
            double frequency = 0;

            /// The amplitude of the frame, if the format records an amplitude.
            std::optional<double> amplitude;

            inline bool operator==(const Frame &RHS) const {
                return time == RHS.time && frequency == RHS.frequency && amplitude == RHS.amplitude;
            }
        };

        /// The distance between consecutive frames in samples, the hop, which is not integral for
        /// some formats, such as dio at a sample rate other than 44100 Hz.
        double hop = 0;

        /// In the order of time.
        std::vector<Frame> frames;

        /// The average frequency in hertz, if the format records an average frequency.
        std::optional<double> averageFrequency;

        /// Returns the average frequency of the frames above 55 Hz on the pitch scale, in
        /// hertz, or the frequency of C4 if no frame is above 55 Hz.
        double pitchAverage() const;

        /// Returns the map at \a count frames of \a hop samples from the start of an audio file
        /// with the sample rate \a sampleRate. The frequency of a frame between two voiced frames
        /// of this map is interpolated linearly in time, and the frequency of a frame next to an
        /// unvoiced frame is that of the nearer frame, so that no frequency arises between a
        /// voiced and an unvoiced frame. The amplitudes are interpolated if every frame has one. A
        /// frame before the first or after the last frame has the values of the nearest frame. The
        /// average frequency is kept.
        FrequencyMap resampled(double hop, int sampleRate, std::size_t count) const;
    };

}

#endif // HELLOKIT_VOICEBANK_FREQUENCYMAP_H
