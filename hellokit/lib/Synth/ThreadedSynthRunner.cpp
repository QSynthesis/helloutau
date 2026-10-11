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

#include <hellokit/Support/TemporaryStorage.h>

#include "SynthToolProcess.h"
#include "ClassicSynthRunner.h"

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
            QString synthToolOutput;
            bool started = false;
        };

    }

    ThreadedSynthRunner::ThreadedSynthRunner() = default;

    ThreadedSynthRunner::~ThreadedSynthRunner() = default;

    SynthOutcome ThreadedSynthRunner::render(const SynthPlan &plan, const SynthTools &synthTools,
                                             SynthObserver *observer,
                                             DiagnosticList &diagnostics) const {
        SynthOutcome outcome;
        if (plan.steps().isEmpty()) {
            fail(diagnostics, tr("There is nothing to render."));
            return outcome;
        }

        // The scripts are generated even if no directory is given, so that a plan that the
        // classic strategy refuses is refused here as well. They are written only into a given
        // directory, for the synth tools that read them. Without one, the paths in the scripts
        // name the temporary storage, and no directory is created.
        const auto directory =
            scriptDirectory.empty() ? TemporaryStorage::location() : scriptDirectory;
        const auto scripts =
            ClassicSynthRunner().scriptFiles(directory, plan, synthTools, diagnostics);
        if (!scripts) {
            return outcome;
        }
        if (!scriptDirectory.empty()) {
            std::error_code scriptError;
            fs::create_directories(scriptDirectory, scriptError);
            if (scriptError || !ClassicSynthRunner::writeScriptFiles(*scripts)) {
                fail(diagnostics, tr("The rendering scripts could not be written."));
                return outcome;
            }
        }

        std::error_code error;
        fs::create_directories(plan.cacheDirectory(), error);
        if (error) {
            fail(diagnostics, tr("The cache folder \"%1\" could not be created.")
                                  .arg(displayed(plan.cacheDirectory())));
            return outcome;
        }

        forgetSuperseded(plan, diagnostics);

        // Determined before any synth tool runs, because the resampler creates these files.
        QList<bool> alreadyThere(int(plan.steps().size()), false);
        if (reuseCache) {
            for (int i = 0; i < int(plan.steps().size()); ++i) {
                alreadyThere[i] =
                    plan.steps().at(i).resamples() && fs::exists(plan.steps().at(i).cacheFile);
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
        // stopped prevents further notes from starting. aborted also kills the running calls,
        // and only a cancellation sets it, because a failure under stopOnFirstFailure leaves the
        // other notes intact.
        std::atomic_bool stopped{false};
        std::atomic_bool aborted{false};
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
        const auto synthTool = makeSynthToolProcess();
        if (!scriptDirectory.empty()) {
            synthTool->workingDirectory = scriptDirectory;
        }

        // The resampler calls are mutually independent and dominate render time. The wavtool
        // calls below append to a single file and always run in track order.
        QList<ResampleOutcome> outcomes(total);
        // The first step of each note that requires no resampler call is complete before the
        // resampler calls start.
        int skipped = 0;
        for (int i = 0; i < total; ++i) {
            skipped += !steps.at(i).resamples() || alreadyThere.at(i) ? 1 : 0;
        }
        report(skipped);
        {
            QThreadPool pool;
            pool.setMaxThreadCount(threadCount > 0 ? threadCount
                                                   : std::max(1, QThread::idealThreadCount()));

            for (int i = 0; i < total; ++i) {
                if (!steps.at(i).resamples() || alreadyThere.at(i)) {
                    continue;
                }
                pool.start([&, i] {
                    if (stopped.load()) {
                        return;
                    }
                    auto &result = outcomes[i];
                    const auto run =
                        synthTool->run(synthTools.resampler, steps.at(i).resamplerArguments,
                                       result.diagnostics, [&] { return aborted.load(); });
                    result.started = run.started;
                    result.synthToolOutput = run.output.trimmed();

                    // A killed resampler may have written part of the fragment, which the next
                    // render would reuse as complete.
                    if (run.cancelled || run.timedOut) {
                        std::error_code removeError;
                        fs::remove(steps.at(i).cacheFile, removeError);
                    }

                    // Success is determined by the existence of the fragment, not by the exit
                    // code, because synth tools report inconsistently and some report nothing.
                    if (stopOnFirstFailure && !fs::exists(steps.at(i).cacheFile)) {
                        stopped.store(true);
                    }
                    report(1);
                });
            }

            // Queried while the pool runs, so that a cancelled render starts no further notes and
            // kills the running calls.
            while (!pool.waitForDone(50)) {
                if (cancelled()) {
                    stopped.store(true);
                    aborted.store(true);
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
            if (step.direct) {
                ++outcome.direct;
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
                         .arg(outcomes.at(i).synthToolOutput.isEmpty()
                                  ? tr("the resampler produced no output.")
                                  : outcomes.at(i).synthToolOutput),
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
            if (!step.resamples() || fs::exists(step.cacheFile)) {
                const auto run = synthTool->run(synthTools.wavtool, step.wavtoolArguments,
                                                diagnostics, cancelled);
                if (run.cancelled) {
                    fs::remove(header, error);
                    fs::remove(data, error);
                    outcome.cancelled = true;
                    return outcome;
                }
                if (!run.started) {
                    return outcome;
                }
            }
            report(1);
        }

        // Some synth tools used as wavtools write the final track directly instead of producing the
        // two files used by the standard UTAU wavtool protocol. As in temp.bat, the two files
        // replace that track if both exist, and the track is kept otherwise.
        if (!fs::exists(header) || !fs::exists(data)) {
            if (fs::exists(output)) {
                outcome.rendered = true;
                return outcome;
            }
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
