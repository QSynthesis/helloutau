#ifndef HELLOKIT_VOICEBANK_FREQUENCYMAPCHECK_H
#define HELLOKIT_VOICEBANK_FREQUENCYMAPCHECK_H

#include <cstddef>
#include <vector>

#include <hellokit/AudioFile/WaveAudio.h>

#include <hellokit/VoiceBank/FrequencyMap.h>

#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>

namespace hello::kit {

    /// The check of a frequency map against the note at which its audio file was recorded, as
    /// the check of frqeditor does it. See the check and the correction in
    /// docs/FrequencyMaps.md.
    ///
    /// The check works on segments, the runs of frames loud enough to be sung, see segments().
    /// In each segment it proposes a correction for two kinds of frames:
    /// - A voiced frame outside the range of the note gets the multiple of its frequency nearest
    ///   to a frequency interpolated between the frames around it, see OutOfRange.
    /// - An unvoiced frame between the first and the last voiced frame of the segment gets a
    ///   frequency interpolated between the frames around it, see UnvoicedFrame.
    ///
    /// A frame is voiced if its frequency is above 55 hertz.
    class HELLOKIT_VOICEBANK_EXPORT FrequencyMapCheck {
    public:
        struct Settings {
            /// The note at which the audio file was recorded, where 60 is C4
            int note = 60;

            /// Whether the audio has some change of pitch, such as a vibrato, which widens the
            /// accepted range from 5 to 10 semitones on either side of the note.
            bool allowsPitchChange = false;

            /// Whether a large change of pitch at the start or at the end of a segment is
            /// accepted. The corrections from the first voiced frame of a segment, or back from
            /// the last, up to the first frame without a correction are dropped.
            bool allowsChangeAtStart = false;
            bool allowsChangeAtEnd = false;
        };

        /// The reason of a correction
        enum Reason {
            NoCorrection,  ///< The frame needs no correction.
            UnvoicedFrame, ///< An unvoiced frame between voiced frames
            OutOfRange,    ///< A voiced frame outside the range of the note
        };

        /// The correction of a frame
        struct Correction {
            Reason reason = NoCorrection;

            /// The proposed frequency in hertz, not above 0 if none was found
            double frequency = 0;
        };

        /// A run of frames, both included
        struct Run {
            std::size_t first = 0;
            std::size_t last = 0;

            bool operator==(const Run &RHS) const {
                return first == RHS.first && last == RHS.last;
            }
        };

        /// Returns the amplitude from which a pair of frames belongs to a segment: of the louder
        /// frame of each pair of frames, the value at a quarter of the ascending order, at least
        /// 500 and at most 2000. Returns 0 if the map has fewer than two frames.
        static double segmentThreshold(const FrequencyMap &map);

        /// Returns the segments of \a map in their order. A pair of frames, frames 0 and 1, 2
        /// and 3 and so on, belongs to a segment if the louder frame reaches segmentThreshold(),
        /// and so does the last frame of an odd number of frames by itself. A segment extends
        /// over the voiced frames next to it in either direction.
        static std::vector<Run> segments(const FrequencyMap &map);

        /// Returns the correction of each frame of \a map, checked against \a settings. A frame
        /// without an amplitude counts as silent, see withAmplitudes().
        static std::vector<Correction> check(const FrequencyMap &map, const Settings &settings);

        /// Returns \a map with an amplitude for each frame that has none, as frqeditor computes
        /// it for the formats that record no amplitude: the root mean square of the first channel
        /// of \a audio over the hop from the start of the frame, in the units of 16-bit samples.
        /// The hop of frame i covers the samples from floor(i × hop) to floor((i + 1) × hop). A
        /// frame beyond the audio gets 0.
        static FrequencyMap withAmplitudes(FrequencyMap map, const WaveAudio &audio);
    };

}

#endif // HELLOKIT_VOICEBANK_FREQUENCYMAPCHECK_H
