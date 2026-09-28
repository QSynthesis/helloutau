#ifndef HELLOKIT_SYNTH_SYNTHPLAN_H
#define HELLOKIT_SYNTH_SYNTHPLAN_H

#include <filesystem>
#include <optional>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Document/Project.h>
#include <hellokit/Support/Diagnostic.h>
#include <hellokit/VoiceBank/VoiceBank.h>

#include <hellokit/Synth/HelloKitSynthGlobal.h>

namespace hello::kit {

    /// The engine calls of one note, with all paths resolved and all arguments determined.
    struct SynthStep {
        /// The position of the note in the track, by which a runner processing several notes
        /// concurrently matches a finished job to its note.
        int noteIndex = 0;

        /// Whether the note is silent, because it is a rest or because the voice bank has no
        /// sample for it. The resampler is not run for a silent note.
        bool silent = false;

        /// The sample to read. Empty if \a silent.
        std::filesystem::path sample;

        /// The rendered fragment this note produces.
        std::filesystem::path cacheFile;

        /// The resampler arguments. Empty if \a silent.
        QStringList resamplerArguments;

        /// The wavtool arguments. The wavtool runs for every note, including silent ones,
        /// because a rest supplies the duration of silence in the track.
        QStringList wavtoolArguments;

        /// The timing of the sample in milliseconds, as reconciled with the previous note: the
        /// pre-utterance and the overlap shortened where the previous note is too short for
        /// them, and the start point moved by what the pre-utterance lost.
        double preUtterance = 0;
        double voiceOverlap = 0;
        double startPoint = 0;

        /// The pitch curve in the resampler arguments, in cents, as PitchCurve::values() gives
        /// it.
        QList<int> pitch;
    };

    /// The components of a render, determined without executing anything.
    ///
    /// Separated from execution so that the parts worth testing can be tested without an engine
    /// on disk: which sample a lyric resolves to, the timing between adjacent notes, and the
    /// exact arguments passed to each engine.
    ///
    /// \note Every string passed to an engine is UTF-8, as \c EngineProcess requires. stdutau
    ///       treats these strings as raw bytes because a UST may use any encoding, but no data
    ///       here comes from a UST: the project is already decoded text, so a single encoding
    ///       applies and no conversion is involved.
    class HELLOKIT_SYNTH_EXPORT SynthPlan {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::SynthPlan)
    public:
        struct Options {
            /// The directory for rendered fragments. UTAU places it beside the project file.
            std::filesystem::path cacheDirectory;

            /// The track file assembled by the wavtool.
            std::filesystem::path outputFile;

            /// The notes to render, as a closed range of track indices, or \c std::nullopt for
            /// all notes.
            ///
            /// \note Notes adjacent to the range are still read. Pre-utterance and overlap are
            ///       determined between neighbors, so rendering a selection differs from
            ///       rendering it in isolation.
            std::optional<std::pair<int, int>> range;
        };

        /// \return the plan, or \c std::nullopt if there is nothing to render, with the reason
        ///         in \a diagnostics
        static std::optional<SynthPlan> make(const Project &project, const VoiceBank &bank,
                                             const Options &options, DiagnosticList &diagnostics);

        /// The steps in track order.
        inline const QList<SynthStep> &steps() const {
            return m_steps;
        }

        /// The track file assembled by this plan.
        inline const std::filesystem::path &outputFile() const {
            return m_outputFile;
        }

        /// The directory for rendered fragments. Must exist before a step writes into it.
        inline const std::filesystem::path &cacheDirectory() const {
            return m_cacheDirectory;
        }

        /// The position in the track, in milliseconds from its start, at which the track file
        /// begins: the start of the first note rendered, less its pre-utterance as reconciled
        /// with its neighbors. The wavtool places the sample of a note that far before the note,
        /// and extends the file by each note with the difference of the pre-utterances of the
        /// note and the next one, so this offset holds for every note of the file.
        ///
        /// A player maps a position in the file to the track by adding it.
        inline double startTime() const {
            return m_startTime;
        }

    private:
        SynthPlan() = default;

        QList<SynthStep> m_steps;
        std::filesystem::path m_outputFile;
        std::filesystem::path m_cacheDirectory;
        double m_startTime = 0;
    };

}

#endif // HELLOKIT_SYNTH_SYNTHPLAN_H
