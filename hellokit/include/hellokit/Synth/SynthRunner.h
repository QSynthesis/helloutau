#ifndef HELLOKIT_SYNTH_SYNTHRUNNER_H
#define HELLOKIT_SYNTH_SYNTHRUNNER_H

#include <filesystem>

#include <QtCore/QString>

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

        /// How many notes each thing happened to.
        int resampled = 0;
        int silent = 0;
        int failed = 0;
    };

    /// Runs a plan, note by note, in track order.
    ///
    /// One note at a time for now. The resampler calls do not depend on one another and are
    /// where the time goes, so they are what a later version spreads over threads; the wavtool
    /// calls append to one file and stay in order whatever happens.
    class HELLOKIT_SYNTH_EXPORT SynthRunner {
    public:
        SynthRunner();
        ~SynthRunner();

        /// How long one engine call may take, in milliseconds.
        int timeout = 30000;

        /// Whether a note the engines could not render stops the whole track.
        ///
        /// Off, because a voice bank with one bad sample should still let the rest be heard, and
        /// the diagnostics say which notes were lost.
        bool stopOnFirstFailure = false;

        SynthOutcome render(const SynthPlan &plan, const SynthEngines &engines,
                            DiagnosticList &diagnostics) const;
    };

}

#endif // HELLOKIT_SYNTH_SYNTHRUNNER_H
