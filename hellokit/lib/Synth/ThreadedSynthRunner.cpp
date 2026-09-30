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

#include "EngineProcess.h"

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

        /// The two files to which the wavtool actually appends.
        ///
        /// The wavtool does not write the track file incrementally. It writes the header to one
        /// file and the sample data to another, both named after the track, and the track file
        /// is the concatenation of the two after every note has been appended. The UTAU batch
        /// file ends with a \c copy \c /B that performs exactly this concatenation.
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

        /// The result of one resampler call, retained until the entire pass completes.
        ///
        /// Each job writes only its own element of a preallocated list, so the jobs require no
        /// locking. The diagnostics are merged afterward in track order rather than in arrival
        /// order, because a list ordered by thread completion is not comprehensible to a user.
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

        // Determined before any engine runs, because the resampler creates these files.
        QList<bool> alreadyThere(int(plan.steps().size()), false);
        if (reuseCache) {
            for (int i = 0; i < int(plan.steps().size()); ++i) {
                alreadyThere[i] =
                    !plan.steps().at(i).silent && fs::exists(plan.steps().at(i).cacheFile);
            }
        }

        // The track is built by appending, so existing files must be removed first. Otherwise a
        // second render would be appended to the first.
        const auto &output = plan.outputFile();
        const auto [header, data] = partsOf(output);
        fs::remove(output, error);
        fs::remove(header, error);
        fs::remove(data, error);

        const auto &steps = plan.steps();
        const int total = int(steps.size());

        // All observer notifications pass through here, so that the observer receives one call
        // at a time regardless of the worker thread. Each note counts as two steps, its
        // resampling and its append.
        QMutex lock;
        std::atomic_bool stopped{false};
        int done = 0;

        const auto report = [&](int count) {
            const QMutexLocker locked(&lock);
            done += count;
            if (observer) {
                observer->progressed(done, 2 * total);
            }
        };
        const auto cancelled = [&] {
            const QMutexLocker locked(&lock);
            return observer && observer->cancelled();
        };

        // Shared by all threads and by the subsequent wavtool calls, which is why it must be
        // safe for concurrent use.
        const auto engine = makeEngineProcess();

        // The resampler calls are mutually independent and dominate render time. The wavtool
        // calls below append to a single file and always run in track order.
        QList<ResampleOutcome> outcomes(total);
        // The first step of each note that requires no resampler call is complete before the
        // resampler calls start.
        int skipped = 0;
        for (int i = 0; i < total; ++i) {
            skipped += steps.at(i).silent || alreadyThere.at(i) ? 1 : 0;
        }
        report(skipped);
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
                    const auto run = engine->run(engines.resampler, steps.at(i).resamplerArguments,
                                                 result.diagnostics);
                    result.started = run.started;
                    result.engineOutput = run.output.trimmed();

                    // Success is determined by the existence of the fragment, not by the exit
                    // code, because engines report inconsistently and some report nothing.
                    if (stopOnFirstFailure && !fs::exists(steps.at(i).cacheFile)) {
                        stopped.store(true);
                    }
                    report(1);
                });
            }

            // Queried while the pool runs, so that a cancelled render starts no further notes.
            // Running calls are allowed to finish, because killing an engine midway would leave
            // a partially written fragment in the cache that the next render would reuse.
            while (!pool.waitForDone(50)) {
                if (cancelled()) {
                    stopped.store(true);
                }
            }
        }

        // In track order, not in thread completion order.
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
                                  ? tr("the resampler produced no output.")
                                  : outcomes.at(i).engineOutput),
                     step.noteIndex);
            if (stopOnFirstFailure) {
                return outcome;
            }
        }

        // These calls append to a single file and therefore run sequentially on one thread. The
        // header and the data of a cancelled render are removed.
        for (const auto &step : steps) {
            if (cancelled()) {
                fs::remove(header, error);
                fs::remove(data, error);
                outcome.cancelled = true;
                return outcome;
            }
            if (step.silent || fs::exists(step.cacheFile)) {
                const auto run = engine->run(engines.wavtool, step.wavtoolArguments, diagnostics);
                if (!run.started) {
                    return outcome;
                }
            }
            report(1);
        }

        if (!fs::exists(header) || !fs::exists(data)) {
            fail(diagnostics, tr("The wavtool produced no output for \"%1\". The configured "
                                 "wavtool may not accept these arguments.")
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
