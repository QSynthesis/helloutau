#ifndef HELLOKIT_SYNTH_SYNTHRUNNER_H
#define HELLOKIT_SYNTH_SYNTHRUNNER_H

#include <filesystem>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Synth/HelloKitSynthGlobal.h>
#include <hellokit/Synth/SynthPlan.h>

namespace hello::kit {

    /// The programs that do the rendering.
    ///
    /// \warning Whatever the host settled on, never what the project named. \c Tool1 and
    ///          \c Tool2 are paths a file chose, and running those without asking is letting the
    ///          file decide which program runs. See AGENTS.md.
    struct SynthEngines {
        std::filesystem::path resampler;
        std::filesystem::path wavtool;
    };

    /// What a render came to.
    struct SynthOutcome {
        /// Whether the track file was written.
        bool rendered = false;

        /// Whether it stopped because it was asked to. Not a failure, and not to be reported as
        /// one.
        bool cancelled = false;

        /// How many notes each thing happened to.
        int resampled = 0;

        /// How many were already rendered and were not rendered again.
        int reused = 0;
        int silent = 0;
        int failed = 0;
    };

    /// Told how a render is going, and asked whether to carry on.
    ///
    /// An interface rather than a callback because the realtime runner has more than one thing
    /// to ask. Implemented wherever there is a user interface, which is not here: this library
    /// does not link QtWidgets.
    ///
    /// \warning Called from whichever thread the work is on, which is not the caller's once a
    ///          runner spreads notes over several. Keeping that straight is the implementor's
    ///          job, the same way it is for \c InterchangeSelector.
    class HELLOKIT_SYNTH_EXPORT SynthObserver {
    public:
        virtual ~SynthObserver();

        /// \a done of \a total notes have been dealt with.
        virtual void progressed(int done, int total);

        /// Asked between notes. Returning true stops the render once what is already running
        /// finishes, and the outcome says it was cancelled rather than that it failed.
        virtual bool cancelled();
    };

    /// Runs a plan.
    ///
    /// The three that exist are not implementation details of one another: which one is used is
    /// a compatibility choice a user makes, and they differ in how the engines are started and
    /// what they are started in, not in what comes out.
    ///
    /// - \c ClassicSynthRunner writes UTAU's own \c temp.bat and runs that, with a visible
    ///   console. Some resamplers need the script to exist and read what is in it.
    /// - \c ThreadedSynthRunner spreads the resampler calls over threads and appends in order.
    /// - \c RealtimeSynthRunner renders around the playback position and drops what is no
    ///   longer wanted.
    ///
    /// \sa docs/Synth.md
    class HELLOKIT_SYNTH_EXPORT SynthRunner {
    public:
        SynthRunner();
        virtual ~SynthRunner();

        /// How long one engine call may take, in milliseconds.
        int timeout = 30000;

        /// Whether a note whose piece is already in the cache folder is left alone.
        ///
        /// On, because the resampler is where a render's time goes and most of a track does not
        /// change between two renders of it. It is safe to have on because the piece's name
        /// stands for everything that decides what is in it: change the note and it is a
        /// different name, so there is nothing there to reuse. See \c SynthPlan.
        ///
        /// Turn it off to render everything again, which is what to do when the engine itself
        /// has been changed or is suspected.
        bool reuseCache = true;

        /// Whether a note the engines could not render stops the whole track.
        ///
        /// Off, because a voice bank with one bad sample should still let the rest be heard, and
        /// the diagnostics say which notes were lost.
        bool stopOnFirstFailure = false;

        /// \param observer may be null, which is what the command line tools and the tests pass
        virtual SynthOutcome render(const SynthPlan &plan, const SynthEngines &engines,
                                    SynthObserver *observer, DiagnosticList &diagnostics) const = 0;

    protected:
        /// Removes the pieces in the cache folder that these notes rendered to before something
        /// about them changed.
        ///
        /// Without this the folder keeps every piece every edit ever produced. A note is
        /// identified by the number its name starts with, which is its place in the track, so
        /// what goes is the pieces sharing a number with a note in the plan and not being the
        /// one that note wants now.
        ///
        /// \note A note that moved in the track leaves its old piece behind under the number it
        ///       used to have. UTAU has the same hole and it is not worth a file of its own to
        ///       plug: the folder is the project's, and emptying it costs one render.
        ///
        /// \return how many were removed
        int forgetSuperseded(const SynthPlan &plan, DiagnosticList &diagnostics) const;
    };

}

#endif // HELLOKIT_SYNTH_SYNTHRUNNER_H
