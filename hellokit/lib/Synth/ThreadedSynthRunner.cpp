#include "ThreadedSynthRunner.h"

#include <fstream>
#include <system_error>

#include <QtCore/QCoreApplication>

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
            fail(diagnostics,
                 tr("The cache folder \"%1\" could not be created.")
                     .arg(displayed(plan.cacheDirectory())));
            return outcome;
        }

        // The track is built up by appending, so whatever was there before has to go first, or a
        // second render lands on the end of the first.
        const auto &output = plan.outputFile();
        const auto [header, data] = partsOf(output);
        fs::remove(output, error);
        fs::remove(header, error);
        fs::remove(data, error);

        EngineProcess engine;
        engine.timeout = timeout;

        int done = 0;
        for (const auto &step : plan.steps()) {
            // Between notes rather than inside one. An engine already running is left to finish,
            // since killing it would leave a half written piece in the cache.
            if (observer && observer->cancelled()) {
                outcome.cancelled = true;
                return outcome;
            }

            if (step.silent) {
                ++outcome.silent;
            } else {
                const auto run =
                    engine.run(engines.resampler, step.resamplerArguments, diagnostics);

                // Not the exit code. Engines disagree about what they report, and some say
                // nothing at all, so what settles it is whether the piece appeared.
                if (!fs::exists(step.cacheFile)) {
                    ++outcome.failed;
                    complain(diagnostics,
                             tr("This note could not be rendered: %1")
                                 .arg(run.output.trimmed().isEmpty()
                                          ? tr("the resampler wrote nothing.")
                                          : run.output.trimmed()),
                             step.noteIndex);
                    if (stopOnFirstFailure) {
                        return outcome;
                    }
                    continue;
                }
                ++outcome.resampled;
            }

            const auto run = engine.run(engines.wavtool, step.wavtoolArguments, diagnostics);
            if (!run.started) {
                return outcome;
            }

            if (observer) {
                observer->progressed(++done, int(plan.steps().size()));
            }
        }

        if (!fs::exists(header) || !fs::exists(data)) {
            fail(diagnostics, tr(
                                  "The wavtool wrote nothing for \"%1\". It may be a different "
                                  "wavtool from the one these arguments are for.")
                                  .arg(displayed(output)));
            return outcome;
        }

        {
            std::ofstream out(output, std::ios::binary | std::ios::trunc);
            if (!out || !append(out, header) || !append(out, data)) {
                fail(
                    diagnostics,
                    tr("\"%1\" could not be written.").arg(displayed(output)));
                return outcome;
            }
        }

        fs::remove(header, error);
        fs::remove(data, error);

        outcome.rendered = true;
        return outcome;
    }

}
