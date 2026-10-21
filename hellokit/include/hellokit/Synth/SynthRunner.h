#ifndef HELLOKIT_SYNTH_SYNTHRUNNER_H
#define HELLOKIT_SYNTH_SYNTHRUNNER_H

#include <filesystem>
#include <memory>
#include <optional>

#include <QtCore/QList>

#include <hellokit/Support/Diagnostic.h>
#include <hellokit/Support/FileIntegrity.h>

#include <hellokit/Synth/SynthFailure.h>
#include <hellokit/Synth/SynthToolProcess.h>
#include <hellokit/Synth/HelloKitSynthGlobal.h>
#include <hellokit/Synth/SynthPlan.h>

namespace hello::kit {

    /// The rendering programs.
    ///
    /// \warning Always the synth tools configured by the host, never those specified by the
    ///          project. \c Tool1 and \c Tool2 are paths chosen by a file, and executing them
    ///          without confirmation lets the file choose which program runs.
    /// \sa CLAUDE.md
    struct SynthTools {
        std::filesystem::path resampler;
        std::filesystem::path wavtool;

        /// The synth tools that run only because the user trusted them, with the SHA-256 that
        /// they had then. A runner holds them unchanged for one render and stops the render if
        /// one changes, see docs/ProgramIntegrity.md. Empty for the synth tools of the settings,
        /// which require no trust.
        QList<FileGuard::File> trusted;
    };

    /// The result of a render.
    struct SynthOutcome {
        /// Whether the track file was written.
        bool rendered = false;

        /// Whether the render stopped on request. Not a failure, and not to be reported as a
        /// failure.
        bool canceled = false;

        /// \name Note counts by outcome
        /// @{
        int resampled = 0;

        /// Notes already rendered and therefore not rendered again.
        int reused = 0;
        int silent = 0;
        int failed = 0;

        /// Notes appended from a file without the resampler, for \c $patch or \c $direct.
        int direct = 0;
        /// @}

        /// The calls whose failure was observed, in track order, for the diagnosis. A render
        /// that succeeds examines no fragment. See docs/SynthDiagnostics.md.
        QList<SynthFailure> failures;
    };

    /// Receives render progress and indicates whether the render continues.
    ///
    /// An interface rather than a callback because the realtime runner has several queries.
    /// Implemented by the user interface layer, not by this library, which does not link
    /// QtWidgets.
    ///
    /// \warning Called on the worker thread, which is not the caller's thread once a runner
    ///          distributes notes over several threads. Thread safety is the responsibility of
    ///          the implementation, as for \c InterchangeSelector.
    class HELLOKIT_SYNTH_EXPORT SynthObserver {
    public:
        virtual ~SynthObserver();

        /// Receives the progress of the render: \a done of \a total steps are complete. Each
        /// runner defines its steps. \a total is constant within one render.
        virtual void progressed(int done, int total);

        /// Queried between notes. Returning true stops the render once the running calls
        /// finish, and the outcome reports cancellation rather than failure.
        virtual bool canceled();
    };

    /// Executes a plan.
    ///
    /// The three implementations are not implementation details of one another. The choice
    /// among them is a compatibility setting made by the user, and they differ in how the synth
    /// tools are started and in the environment in which they are started, not in their output.
    ///
    /// - \c ClassicSynthRunner writes the UTAU \c temp.bat and runs it in a visible console.
    ///   Some resamplers require the script to exist and read its contents.
    /// - \c ThreadedSynthRunner distributes the resampler calls over threads and appends the
    ///   results in order.
    /// - \c RealtimeSynthRunner renders around the playback position and discards results that
    ///   are no longer needed.
    ///
    /// \sa docs/Synth.md
    class HELLOKIT_SYNTH_EXPORT SynthRunner {
    public:
        SynthRunner();
        virtual ~SynthRunner();

        /// The time limit of one synth tool call, in milliseconds.
        int timeout = 30000;

        /// Whether a note whose fragment already exists in the cache directory is skipped.
        ///
        /// Enabled by default, because the resampler dominates render time and most of a track
        /// is unchanged between two renders. This is safe because the fragment name encodes
        /// every input that determines its content: a changed note has a different name, so no
        /// stale fragment can be reused.
        ///
        /// Disable to render everything again, for example after the synth tool has changed or is
        /// suspected of faulty output.
        ///
        /// \sa SynthPlan
        bool reuseCache = true;

        /// Whether a note that the synth tools fail to render stops the entire track.
        ///
        /// Disabled by default, because a voice bank with one defective sample should still
        /// allow the remainder to be heard, and the diagnostics identify the lost notes.
        bool stopOnFirstFailure = false;

        /// \param observer may be null, as passed by the command-line tools and the tests
        virtual SynthOutcome render(const SynthPlan &plan, const SynthTools &synthTools,
                                    SynthObserver *observer, DiagnosticList &diagnostics) const = 0;

        void setOutputLog(std::shared_ptr<SynthToolOutputLog> outputLog) const;

        /// Holds the trusted synth tools of \a synthTools while they run. A synth tool that has
        /// changed since it was trusted, or cannot be read, is reported to \a diagnostics. Every
        /// runner and RealtimeSynth hold them through this function.
        ///
        /// \return the guard, which holds no file if no synth tool is trusted, or
        ///         \c std::nullopt if the synth tools must not run
        static std::optional<FileGuard> holdTrusted(const SynthTools &synthTools,
                                                    DiagnosticList &diagnostics);

        /// Returns whether a synth tool of \a guard has changed while it was held, and reports
        /// the change to \a diagnostics if so. A render stopped by a change is a failure, not a
        /// cancellation.
        static bool reportChange(const FileGuard &guard, DiagnosticList &diagnostics);

    protected:
        /// Creates the object that starts one synth tool.
        ///
        /// The test seam. A test substitutes its own synth tool by overriding this function, which
        /// is the only way to cover the behavior of a runner after the arguments are passed:
        /// joining the two fragments the wavtool writes, clearing the remains of a previous
        /// render, counting a note whose fragment never appeared, and stopping on failure.
        ///
        /// \note One instance is shared by all threads of a runner, so the returned object must
        ///       be safe to call concurrently. The default implementation is, because it keeps
        ///       no state between calls.
        virtual std::unique_ptr<SynthToolProcess> makeSynthToolProcess() const;

        /// Removes the fragments in the cache directory that these notes rendered before their
        /// inputs changed.
        ///
        /// Without this, the directory accumulates a fragment for every edit. A note is
        /// identified by the number at the start of the fragment name, which is its position in
        /// the track. The removed fragments are those that share a number with a note in the
        /// plan but differ from the fragment that note currently requires.
        ///
        /// \note A note that moved within the track leaves its old fragment behind under its
        ///       former number. UTAU has the same limitation, and it does not justify a separate
        ///       index file: the directory belongs to the project, and clearing it costs one
        ///       render.
        ///
        /// \return the number of removed fragments
        int removeSuperseded(const SynthPlan &plan, DiagnosticList &diagnostics) const;

        /// Returns the record of the resampler call of \a step, with \a run if known, if the
        /// fragment does not exist, or \c std::nullopt otherwise. The content of an existing
        /// fragment is not examined, because a resampler may write an intermediate format that
        /// its own wavtool joins.
        static std::optional<SynthFailure> resamplerFailure(const SynthStep &step,
                                                            const SynthTools &synthTools,
                                                            std::optional<SynthToolRun> run);

    private:
        mutable std::shared_ptr<SynthToolOutputLog> m_outputLog;
    };

}

#endif // HELLOKIT_SYNTH_SYNTHRUNNER_H
