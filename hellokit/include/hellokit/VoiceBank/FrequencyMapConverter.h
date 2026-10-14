#ifndef HELLOKIT_VOICEBANK_FREQUENCYMAPCONVERTER_H
#define HELLOKIT_VOICEBANK_FREQUENCYMAPCONVERTER_H

#include <filesystem>

#include <QtCore/QList>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/VoiceBank/FrequencyFormat.h>
#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>

namespace hello::kit {

    struct WaveAudio;

    /// Converts frequency maps from one format to another by reading each map with the source
    /// format and writing it with the target format. See the conversion in docs/FrequencyMaps.md.
    class HELLOKIT_VOICEBANK_EXPORT FrequencyMapConverter {
    public:
        /// The result of convert().
        struct Outcome {
            /// The number of maps written
            int converted = 0;
            /// The number of maps written after resampling to the hop of the target format
            int resampled = 0;
        };

        /// Returns \a map of the audio \a audio in the terms of \a target. A map whose hop
        /// differs from FrequencyFormat::hopOf() of \a target at the sample rate of \a audio is
        /// resampled with FrequencyMap::resampled() to floor(samples / hop) + 1 frames, the frame
        /// count of the resamplers of UTAU and world4utau. If a frame lacks an amplitude, the
        /// amplitude of every frame is the root mean square of the samples of the first channel
        /// of \a audio in the frame, on the scale of 16-bit samples.
        static FrequencyMap converted(const FrequencyMap &map, const FrequencyFormat &target,
                                      const WaveAudio &audio);

        /// Converts the maps of \a source of the audio files \a wavs, names in the directory
        /// \a directory, to \a target with converted(), and writes them with
        /// FrequencyFormats::writeMaps(), which replaces the maps of \a target and removes their
        /// analysis caches. An audio file without a map of \a source is skipped. An audio file or
        /// a map that cannot be read is reported in \a diagnostics, and the other maps are
        /// converted.
        static Outcome convert(const FrequencyFormat &source, const FrequencyFormat &target,
                               const std::filesystem::path &directory,
                               const QList<std::filesystem::path> &wavs,
                               DiagnosticList &diagnostics);
    };

}

#endif // HELLOKIT_VOICEBANK_FREQUENCYMAPCONVERTER_H
