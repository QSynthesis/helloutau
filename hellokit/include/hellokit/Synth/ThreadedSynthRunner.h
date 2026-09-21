#ifndef HELLOKIT_SYNTH_THREADEDSYNTHRUNNER_H
#define HELLOKIT_SYNTH_THREADEDSYNTHRUNNER_H

#include <QtCore/QCoreApplication>

#include <hellokit/Synth/HelloKitSynthGlobal.h>
#include <hellokit/Synth/SynthRunner.h>

namespace hello::kit {

    /// Renders a whole track and returns when it is done.
    ///
    /// The everyday one. The resampler calls do not depend on one another and are where the time
    /// goes, so they are what gets spread over threads; the wavtool calls append to one file and
    /// stay in track order whatever else happens. That is also how UTAU's own multi-core mode
    /// works: it runs the resamplers from several scripts and leaves the appending to a last one.
    ///
    /// \note **It does not spread anything over threads yet.** Today it runs one note at a time,
    ///       which is that design with one worker. What the threading needs first is somewhere
    ///       to put an engine other than \c EngineProcess: a pool has to own the calls, and a
    ///       test has nowhere to stand until the same seam exists. See
    ///       tests/auto/Synth/test_ThreadedSynthRunner.cpp.
    ///
    /// \sa docs/Synth.md
    class HELLOKIT_SYNTH_EXPORT ThreadedSynthRunner : public SynthRunner {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::ThreadedSynthRunner)
    public:
        ThreadedSynthRunner();
        ~ThreadedSynthRunner() override;

        SynthOutcome render(const SynthPlan &plan, const SynthEngines &engines,
                            SynthObserver *observer, DiagnosticList &diagnostics) const override;
    };

}

#endif // HELLOKIT_SYNTH_THREADEDSYNTHRUNNER_H
