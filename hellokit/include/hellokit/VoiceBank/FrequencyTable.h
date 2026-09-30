#ifndef HELLOKIT_VOICEBANK_FREQUENCYTABLE_H
#define HELLOKIT_VOICEBANK_FREQUENCYTABLE_H

#include <optional>
#include <vector>

namespace hello::kit {

    /// The fundamental frequency of an audio file as a resampler analyzed it, in the terms that
    /// every format shares. See docs/FrequencyTables.md.
    ///
    /// A value read from a file, for display: what a format records beyond these terms is not
    /// kept.
    struct FrequencyTable {
        struct Frame {
            /// Milliseconds from the start of the audio file.
            double time = 0;

            /// Hertz, or 0 for a frame that the format marks unvoiced.
            double frequency = 0;

            /// The amplitude of the frame, if the format records one.
            std::optional<double> amplitude;

            inline bool operator==(const Frame &RHS) const {
                return time == RHS.time && frequency == RHS.frequency && amplitude == RHS.amplitude;
            }
        };

        /// In the order of time.
        std::vector<Frame> frames;

        /// The average frequency in hertz, if the format records one.
        std::optional<double> averageFrequency;
    };

}

#endif // HELLOKIT_VOICEBANK_FREQUENCYTABLE_H
