#ifndef HELLOKIT_SYNTH_THREADEDSYNTHRUNNER_H
#define HELLOKIT_SYNTH_THREADEDSYNTHRUNNER_H

#include <QtCore/QCoreApplication>

#include <hellokit/Synth/HelloKitSynthGlobal.h>
#include <hellokit/Synth/SynthRunner.h>

namespace hello::kit {

    /// Renders an entire track and returns on completion.
    ///
    /// The default runner. The resampler calls are mutually independent and dominate render
    /// time, so they are distributed over threads. The wavtool calls append to a single file and
    /// always run in track order. The multi-core mode of UTAU works the same way: it runs the
    /// resamplers from several scripts and leaves appending to a final one.
    ///
    /// Progress is reported in two steps per note: its resampling and its append to the track.
    /// The first step of a silent note, or of a note whose fragment is reused, is complete before
    /// any engine runs. A render from a full cache therefore reports progress for each append. A
    /// cancellation takes effect between the resampler calls and between the wavtool calls.
    ///
    /// \sa docs/Synth.md
    class HELLOKIT_SYNTH_EXPORT ThreadedSynthRunner : public SynthRunner {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::ThreadedSynthRunner)
    public:
        ThreadedSynthRunner();
        ~ThreadedSynthRunner() override;

        /// The number of concurrent resampler calls, or zero for one per hardware thread.
        ///
        /// \note Only resampling is parallelized. The wavtool appends to a single file, so its
        ///       calls run sequentially regardless of this setting.
        int threadCount = 0;

        SynthOutcome render(const SynthPlan &plan, const SynthEngines &engines,
                            SynthObserver *observer, DiagnosticList &diagnostics) const override;
    };

}

#endif // HELLOKIT_SYNTH_THREADEDSYNTHRUNNER_H
