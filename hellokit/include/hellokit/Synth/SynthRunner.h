#ifndef HELLOKIT_SYNTH_SYNTHRUNNER_H
#define HELLOKIT_SYNTH_SYNTHRUNNER_H

#include <filesystem>
#include <memory>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Synth/EngineProcess.h>
#include <hellokit/Synth/HelloKitSynthGlobal.h>
#include <hellokit/Synth/SynthPlan.h>

namespace hello::kit {

    /// The rendering programs.
    ///
    /// \warning Always the engines configured by the host, never those specified by the
    ///          project. \c Tool1 and \c Tool2 are paths chosen by a file, and executing them
    ///          without confirmation lets the file choose which program runs.
    /// \sa CLAUDE.md
    struct SynthEngines {
        std::filesystem::path resampler;
        std::filesystem::path wavtool;
    };

    /// The result of a render.
    struct SynthOutcome {
        /// Whether the track file was written.
        bool rendered = false;

        /// Whether the render stopped on request. Not a failure, and not to be reported as one.
        bool cancelled = false;

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
        virtual bool cancelled();
    };

    /// Executes a plan.
    ///
    /// The three implementations are not implementation details of one another. The choice
    /// among them is a compatibility setting made by the user, and they differ in how and in
    /// what environment the engines are started, not in their output.
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

        /// The time limit of one engine call, in milliseconds.
        int timeout = 30000;

        /// Whether a note whose fragment already exists in the cache directory is skipped.
        ///
        /// Enabled by default, because the resampler dominates render time and most of a track
        /// is unchanged between two renders. This is safe because the fragment name encodes
        /// every input that determines its content: a changed note has a different name, so no
        /// stale fragment can be reused.
        ///
        /// Disable to render everything again, for example after the engine has changed or is
        /// suspected of faulty output.
        ///
        /// \sa SynthPlan
        bool reuseCache = true;

        /// Whether a note that the engines fail to render stops the entire track.
        ///
        /// Disabled by default, because a voice bank with one defective sample should still
        /// allow the remainder to be heard, and the diagnostics identify the lost notes.
        bool stopOnFirstFailure = false;

        /// \param observer may be null, as passed by the command-line tools and the tests
        virtual SynthOutcome render(const SynthPlan &plan, const SynthEngines &engines,
                                    SynthObserver *observer, DiagnosticList &diagnostics) const = 0;

        void setOutputLog(std::shared_ptr<EngineOutputLog> outputLog) const;

    protected:
        /// Creates the object that starts one engine.
        ///
        /// The test seam. A test substitutes its own engine by overriding this function, which
        /// is the only way to cover the behavior of a runner after the arguments are passed:
        /// joining the two fragments the wavtool writes, clearing the remains of a previous
        /// render, counting a note whose fragment never appeared, and stopping on failure.
        ///
        /// \note One instance is shared by all threads of a runner, so the returned object must
        ///       be safe to call concurrently. The default implementation is, because it keeps
        ///       no state between calls.
        virtual std::unique_ptr<EngineProcess> makeEngineProcess() const;

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
        int forgetSuperseded(const SynthPlan &plan, DiagnosticList &diagnostics) const;

    private:
        mutable std::shared_ptr<EngineOutputLog> m_outputLog;
    };

}

#endif // HELLOKIT_SYNTH_SYNTHRUNNER_H
