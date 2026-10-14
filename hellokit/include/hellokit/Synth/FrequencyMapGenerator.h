#ifndef HELLOKIT_SYNTH_FREQUENCYMAPGENERATOR_H
#define HELLOKIT_SYNTH_FREQUENCYMAPGENERATOR_H

#include <filesystem>
#include <functional>
#include <memory>

#include <QtCore/QCoreApplication>
#include <QtCore/QList>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/VoiceBank/FrequencyFormat.h>

#include <hellokit/Synth/HelloKitSynthGlobal.h>
#include <hellokit/Synth/SynthFailure.h>

namespace hello::kit {

    class SynthToolProcess;

    /// Generates the frequency maps of audio files by running a resampler, which writes the map
    /// of an audio file that has none when it renders the audio file. See the generation in
    /// docs/FrequencyMaps.md.
    ///
    /// Each audio file is rendered as a note of 50 milliseconds at C4 into a temporary file that
    /// is removed afterwards. The arguments are those of utau::ResamplerArguments, passed as a
    /// vector. The resampler calls are mutually independent and run on several threads. To
    /// generate a map again, the caller first removes it with FrequencyFormats::removeMaps().
    ///
    /// \warning The resampler is always the synth tool of the settings of the host, never a
    ///          synth tool that a project specifies. See SynthToolProcess.
    class HELLOKIT_SYNTH_EXPORT FrequencyMapGenerator {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::FrequencyMapGenerator)
    public:
        FrequencyMapGenerator();
        ~FrequencyMapGenerator();

        /// The number of concurrent resampler calls, or zero for one per hardware thread.
        int threadCount = 0;

        /// Whether an audio file that has a map is rendered as well, so that the resampler
        /// writes its removed analysis caches again from the map. False by default.
        bool everyFile = false;

        /// The process that runs the resampler, which writes the output of the resampler into its
        /// output log. A test replaces it with a substitute.
        std::shared_ptr<SynthToolProcess> process;

        /// The result of generate().
        struct Outcome {
            /// The number of audio files rendered that have a map afterwards
            int generated = 0;

            /// The resampler calls after which an audio file has no map, for a diagnosis with
            /// the operation SynthDiagnosisContext::FrequencyMapGeneration
            QList<SynthFailure> failures;
        };

        /// Generates the map of \a format of each audio file of \a wavs that has none, or of every
        /// audio file if everyFile is set, by running \a resampler, and returns on completion.
        /// An audio file that has no map afterwards is reported in \a diagnostics, and its
        /// resampler call in the failures of the outcome.
        ///
        /// \param progress called after each audio file with the number of audio files done, on
        ///        a thread of the generation, or empty
        /// \param cancelled queried on the threads of the generation while resamplers run, or
        ///        empty. Once it returns true, the running resamplers are killed and no further
        ///        audio file is started.
        Outcome generate(const FrequencyFormat &format, const std::filesystem::path &resampler,
                         const QList<std::filesystem::path> &wavs, DiagnosticList &diagnostics,
                         const std::function<void(int)> &progress = {},
                         const std::function<bool()> &cancelled = {}) const;
    };

}

#endif // HELLOKIT_SYNTH_FREQUENCYMAPGENERATOR_H
