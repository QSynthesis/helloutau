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

    /// One note's engine calls, with every path resolved and every argument settled.
    struct SynthStep {
        /// Where the note sits in the track, which is what a renderer working several notes at
        /// once matches a finished job back by.
        int noteIndex = 0;

        /// Whether the note makes no sound, because it is a rest or because the bank has
        /// nothing to sing it with. The resampler is not run for one of these.
        bool silent = false;

        /// The sample to read. Empty where \a silent.
        std::filesystem::path sample;

        /// The rendered piece this note produces.
        std::filesystem::path cacheFile;

        /// What to hand the resampler. Empty where \a silent.
        QStringList resamplerArguments;

        /// What to hand the wavtool, which runs for every note, silent ones included: a rest is
        /// how silence gets its length in the track.
        QStringList wavtoolArguments;
    };

    /// What a render is made of, worked out without running anything.
    ///
    /// Kept apart from running it so that the part worth testing can be tested with no engine on
    /// disk: which sample a lyric resolves to, what the timing between neighbours comes to, and
    /// exactly what each engine is handed.
    ///
    /// \note Every string that reaches an engine is UTF-8, which is what \c EngineProcess takes.
    ///       stdutau calls these strings raw bytes because a UST may be in anything, but nothing
    ///       here came from a UST: the project is already text, so there is one encoding on this
    ///       side and no conversion in the middle.
    class HELLOKIT_SYNTH_EXPORT SynthPlan {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::SynthPlan)
    public:
        struct Options {
            /// Where the rendered pieces go. UTAU keeps this beside the project file.
            std::filesystem::path cacheDirectory;

            /// The track file the wavtool builds up.
            std::filesystem::path outputFile;

            /// The notes to render, as a closed range of track indexes, or nothing for all of
            /// them.
            ///
            /// \note A range still reads the notes just outside it. Pre-utterance and overlap
            ///       are settled between neighbours, so rendering a selection is not the same as
            ///       rendering it as though nothing else were there.
            std::optional<std::pair<int, int>> range;
        };

        /// \return the plan, or nothing where there is nothing to render, with the reason in
        ///         \a diagnostics
        static std::optional<SynthPlan> make(const Project &project, const VoiceBank &bank,
                                             const Options &options, DiagnosticList &diagnostics);

        /// The steps in track order.
        const QList<SynthStep> &steps() const {
            return m_steps;
        }

        /// The track file this plan builds up.
        const std::filesystem::path &outputFile() const {
            return m_outputFile;
        }

        /// Where the rendered pieces go. It has to exist before a step can write into it.
        const std::filesystem::path &cacheDirectory() const {
            return m_cacheDirectory;
        }

    private:
        SynthPlan() = default;

        QList<SynthStep> m_steps;
        std::filesystem::path m_outputFile;
        std::filesystem::path m_cacheDirectory;
    };

}

#endif // HELLOKIT_SYNTH_SYNTHPLAN_H
