#include "ThreadedSynthRunner.h"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <system_error>

#include <QtCore/QCoreApplication>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QThread>
#include <QtCore/QThreadPool>

#include <hellokit/Synth/EngineProcess.h>

namespace hello::kit {

    namespace fs = std::filesystem;

    namespace {

        void fail(DiagnosticList &diagnostics, const QString &message,
                  std::optional<int> noteIndex = std::nullopt) {
            diagnostics.push_back({DiagnosticSeverity::Error, message, noteIndex});
        }

        void complain(DiagnosticList &diagnostics, const QString &message, int noteIndex) {
            diagnostics.push_back({DiagnosticSeverity::Warning, message, noteIndex});
        }

        QString displayed(const fs::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

        /// The two files the wavtool actually appends to.
        ///
        /// It does not write the track wav as it goes. It keeps the header in one file and the
        /// samples in another, both named after the track, and the wav is the two joined once
        /// every note has been appended. UTAU's own batch ends with a \c copy \c /B that does
        /// exactly this.
        std::pair<fs::path, fs::path> partsOf(const fs::path &output) {
            auto header = output;
            auto data = output;
            header += ".whd";
            data += ".dat";
            return {header, data};
        }

        bool append(std::ofstream &out, const fs::path &path) {
            std::ifstream in(path, std::ios::binary);
            if (!in) {
                return false;
            }
            out << in.rdbuf();
            return out.good();
        }

        /// What one resampler call came to, kept until the whole pass is over.
        ///
        /// Each job writes only its own entry of a list sized up front, so the jobs need no lock
        /// between them. The diagnostics are merged afterwards in track order rather than as
        /// they arrive, since a list that reads in whatever order the threads happened to finish
        /// is not something a user can follow.
        struct ResampleOutcome {
            DiagnosticList diagnostics;
            QString engineOutput;
            bool started = false;
        };

    }

    ThreadedSynthRunner::ThreadedSynthRunner() = default;

    ThreadedSynthRunner::~ThreadedSynthRunner() = default;

    SynthOutcome ThreadedSynthRunner::render(const SynthPlan &plan, const SynthEngines &engines,
                                             SynthObserver *observer,
                                             DiagnosticList &diagnostics) const {
        SynthOutcome outcome;
        if (plan.steps().isEmpty()) {
            fail(diagnostics, tr("There is nothing to render."));
            return outcome;
        }

        std::error_code error;
        fs::create_directories(plan.cacheDirectory(), error);
        if (error) {
            fail(diagnostics, tr("The cache folder \"%1\" could not be created.")
                                  .arg(displayed(plan.cacheDirectory())));
            return outcome;
        }

        forgetSuperseded(plan, diagnostics);

        // Settled before anything runs, because the resampler is about to start creating these.
        QList<bool> alreadyThere(int(plan.steps().size()), false);
        if (reuseCache) {
            for (int i = 0; i < int(plan.steps().size()); ++i) {
                alreadyThere[i] =
                    !plan.steps().at(i).silent && fs::exists(plan.steps().at(i).cacheFile);
            }
        }

        // The track is built up by appending, so whatever was there before has to go first, or a
        // second render lands on the end of the first.
        const auto &output = plan.outputFile();
        const auto [header, data] = partsOf(output);
        fs::remove(output, error);
        fs::remove(header, error);
        fs::remove(data, error);

        const auto &steps = plan.steps();
        const int total = int(steps.size());

        // Everything the observer is told goes through here, so that it sees one call at a time
        // whichever thread the work was on.
        QMutex lock;
        std::atomic_bool stopped{false};
        int done = 0;

        const auto report = [&] {
            const QMutexLocker locked(&lock);
            if (observer) {
                observer->progressed(++done, total);
            }
        };
        const auto cancelled = [&] {
            const QMutexLocker locked(&lock);
            return observer && observer->cancelled();
        };

        // One for every thread and for the wavtool afterwards, which is why it has to be
        // safe to call from several at once.
        const auto engine = makeEngineProcess();

        // The resampler calls do not depend on one another and are where the time goes. The
        // wavtool calls below append to one file and stay in track order whatever happens here.
        QList<ResampleOutcome> outcomes(total);
        {
            QThreadPool pool;
            pool.setMaxThreadCount(threadCount > 0 ? threadCount
                                                   : std::max(1, QThread::idealThreadCount()));

            for (int i = 0; i < total; ++i) {
                if (steps.at(i).silent || alreadyThere.at(i)) {
                    continue;
                }
                pool.start([&, i] {
                    if (stopped.load()) {
                        return;
                    }
                    auto &result = outcomes[i];
                    const auto run = engine->run(engines.resampler,
                                                 steps.at(i).resamplerArguments,
                                                 result.diagnostics);
                    result.started = run.started;
                    result.engineOutput = run.output.trimmed();

                    // Not the exit code. Engines disagree about what they report, and some say
                    // nothing at all, so what settles it is whether the piece appeared.
                    if (stopOnFirstFailure && !fs::exists(steps.at(i).cacheFile)) {
                        stopped.store(true);
                    }
                    report();
                });
            }

            // Asked while the pool works, so that a render the user gave up on stops starting
            // new notes. What is already running is left to finish: killing an engine part way
            // would leave a half written piece in the cache for the next render to trust.
            while (!pool.waitForDone(50)) {
                if (cancelled()) {
                    stopped.store(true);
                }
            }
        }

        // In track order, not in the order the threads finished.
        for (int i = 0; i < total; ++i) {
            diagnostics.append(outcomes.at(i).diagnostics);
        }

        if (stopped.load() && !stopOnFirstFailure) {
            outcome.cancelled = true;
            return outcome;
        }

        for (int i = 0; i < total; ++i) {
            const auto &step = steps.at(i);
            if (step.silent) {
                ++outcome.silent;
                continue;
            }
            if (alreadyThere.at(i)) {
                ++outcome.reused;
                continue;
            }
            if (fs::exists(step.cacheFile)) {
                ++outcome.resampled;
                continue;
            }
            ++outcome.failed;
            complain(diagnostics,
                     tr("This note could not be rendered: %1")
                         .arg(outcomes.at(i).engineOutput.isEmpty()
                                  ? tr("the resampler wrote nothing.")
                                  : outcomes.at(i).engineOutput),
                     step.noteIndex);
            if (stopOnFirstFailure) {
                return outcome;
            }
        }

        // One file, appended to, so these stay in order and on one thread.
        for (const auto &step : steps) {
            if (!step.silent && !fs::exists(step.cacheFile)) {
                continue;
            }
            const auto run = engine->run(engines.wavtool, step.wavtoolArguments, diagnostics);
            if (!run.started) {
                return outcome;
            }
        }

        if (!fs::exists(header) || !fs::exists(data)) {
            fail(diagnostics, tr("The wavtool wrote nothing for \"%1\". It may be a different "
                                 "wavtool from the one these arguments are for.")
                                  .arg(displayed(output)));
            return outcome;
        }

        {
            std::ofstream out(output, std::ios::binary | std::ios::trunc);
            if (!out || !append(out, header) || !append(out, data)) {
                fail(diagnostics, tr("\"%1\" could not be written.").arg(displayed(output)));
                return outcome;
            }
        }

        fs::remove(header, error);
        fs::remove(data, error);

        outcome.rendered = true;
        return outcome;
    }

}
