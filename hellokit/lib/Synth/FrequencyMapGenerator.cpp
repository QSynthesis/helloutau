#include "FrequencyMapGenerator.h"

#include <algorithm>
#include <atomic>
#include <map>
#include <numeric>

#include <QtCore/QDir>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QTemporaryDir>
#include <QtCore/QThread>
#include <QtCore/QThreadPool>

#include <stdutau/synth.h>

#include <hellokit/Support/TemporaryStorage.h>

#include "SynthToolProcess.h"

namespace hello::kit {

    namespace fs = std::filesystem;

    namespace {

        QString displayed(const fs::path &path) {
            return QDir::toNativeSeparators(QString::fromStdU16String(path.u16string()));
        }

        QStringList listOf(const std::vector<std::string> &arguments) {
            QStringList list;
            for (const auto &argument : arguments) {
                list.push_back(QString::fromUtf8(argument.data(), qsizetype(argument.size())));
            }
            return list;
        }

        // Returns the failure of a resampler call, with the rendered file read while it still
        // exists in the temporary folder.
        SynthFailure failureOf(SynthFailure::Stage stage, const fs::path &resampler,
                               const QStringList &arguments, const SynthToolRun &run,
                               const fs::path &rendered) {
            SynthFailure failure;
            failure.stage = stage;
            failure.program = resampler;
            failure.arguments = arguments;
            failure.run = run;
            failure.file = rendered;
            failure.readFile();
            return failure;
        }

    }

    FrequencyMapGenerator::FrequencyMapGenerator() = default;

    FrequencyMapGenerator::~FrequencyMapGenerator() = default;

    FrequencyMapGenerator::Outcome
        FrequencyMapGenerator::generate(const FrequencyFormat &format, const fs::path &resampler,
                                        const QList<fs::path> &wavs, DiagnosticList &diagnostics,
                                        const std::function<void(int)> &progress,
                                        const std::function<bool()> &canceled) const {
        const auto synthTool = process ? process : std::make_shared<SynthToolProcess>();
        Outcome outcome;
        // The rendered notes, which only make the resampler write the maps, removed with the
        // folder at the end
        QTemporaryDir output(TemporaryStorage::templatePath(QStringLiteral("frequency")));
        if (!output.isValid()) {
            diagnostics.push_back({DiagnosticSeverity::Error,
                                   tr("A temporary folder for the resampler cannot be created.")});
            return outcome;
        }
        const fs::path outputDirectory(output.path().toStdU16String());

        QMutex mutex;
        std::atomic<int> done = 0;
        std::atomic<bool> stopped = false;
        std::atomic<bool> unstarted = false;
        const auto stop = [&] {
            if (!stopped.load() && canceled && canceled()) {
                stopped = true;
            }
            return stopped.load();
        };
        // Returns whether each audio file of indices has a map, by its index, queried by folder.
        const auto mapsOf = [&format, &wavs](const QList<qsizetype> &indices) {
            std::map<fs::path, QList<qsizetype>> byFolder;
            for (const auto i : indices) {
                byFolder[wavs.at(i).parent_path()].push_back(i);
            }
            std::map<qsizetype, bool> present;
            for (const auto &[folder, members] : byFolder) {
                QList<fs::path> names;
                for (const auto i : members) {
                    names.push_back(wavs.at(i).filename());
                }
                const auto found = format.mapsExist(folder, names);
                for (qsizetype k = 0; k < members.size(); ++k) {
                    present[members.at(k)] = found.value(k);
                }
            }
            return present;
        };
        QList<qsizetype> all(wavs.size());
        std::iota(all.begin(), all.end(), 0);
        const auto present = mapsOf(all);

        // Each call of the resampler that ran is checked once every call has finished, because
        // moresampler locks desc.mrq while it writes an entry and a read of the file during the
        // lock fails, measured with moresampler 0.8.4 on 2026-10-14 (docs/FrequencyMaps.md).
        struct Call {
            qsizetype index = 0;
            QStringList arguments;
            fs::path rendered;
            SynthToolRun run;
        };
        QList<Call> calls;
        {
            QThreadPool pool;
            pool.setMaxThreadCount(threadCount > 0 ? threadCount
                                                   : std::max(1, QThread::idealThreadCount()));
            for (qsizetype i = 0; i < wavs.size(); ++i) {
                pool.start([&, i] {
                    const auto &wav = wavs.at(i);
                    if (!stop() && (everyFile || !present.at(i))) {
                        utau::ResamplerArguments arguments;
                        arguments.inFile = wav.u8string();
                        Call call;
                        call.index = i;
                        call.rendered = outputDirectory / (std::to_string(i) + ".wav");
                        arguments.outFile = call.rendered.u8string();
                        arguments.toneName = "C4";
                        arguments.realLength = 50;
                        call.arguments = listOf(arguments.arguments());
                        DiagnosticList found;
                        call.run = synthTool->run(resampler, call.arguments, found, stop);
                        std::optional<SynthFailure> failure;
                        if (!call.run.started) {
                            // Every other call would fail alike.
                            stopped = true;
                            if (!unstarted.exchange(true)) {
                                found.push_back({DiagnosticSeverity::Error,
                                                 tr("The resampler \"%1\" could not be started.")
                                                     .arg(displayed(resampler))});
                                failure = failureOf(SynthFailure::Resampler, resampler,
                                                    call.arguments, call.run, call.rendered);
                            }
                        }
                        const QMutexLocker locker(&mutex);
                        diagnostics += found;
                        if (failure) {
                            outcome.failures.push_back(std::move(*failure));
                        }
                        if (call.run.started && !call.run.canceled) {
                            calls.push_back(std::move(call));
                        }
                    }
                    const int count = ++done;
                    if (progress) {
                        progress(count);
                    }
                });
            }
            pool.waitForDone();
        }

        std::sort(calls.begin(), calls.end(),
                  [](const Call &a, const Call &b) { return a.index < b.index; });
        QList<qsizetype> ran;
        for (const auto &call : std::as_const(calls)) {
            ran.push_back(call.index);
        }
        const auto written = mapsOf(ran);
        for (const auto &call : std::as_const(calls)) {
            if (written.at(call.index)) {
                ++outcome.generated;
                continue;
            }
            // The output of the resampler is in its output log.
            diagnostics.push_back({DiagnosticSeverity::Warning,
                                   tr("The resampler wrote no frequency map of \"%1\" "
                                      "(exit code %2).")
                                       .arg(displayed(wavs.at(call.index)))
                                       .arg(call.run.exitCode)});
            const bool exited = !call.run.timedOut && call.run.exitCode == 0;
            outcome.failures.push_back(
                failureOf(exited ? SynthFailure::FrequencyMap : SynthFailure::Resampler, resampler,
                          call.arguments, call.run, call.rendered));
        }
        return outcome;
    }

}
