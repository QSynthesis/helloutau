#include "FrequencyMapGenerator.h"

#include <algorithm>
#include <atomic>

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

    }

    FrequencyMapGenerator::FrequencyMapGenerator() = default;

    FrequencyMapGenerator::~FrequencyMapGenerator() = default;

    int FrequencyMapGenerator::generate(const FrequencyFormat &format, const fs::path &resampler,
                                        const QList<fs::path> &wavs, DiagnosticList &diagnostics,
                                        const std::function<void(int)> &progress,
                                        const std::function<bool()> &cancelled) const {
        const auto synthTool = process ? process : std::make_shared<SynthToolProcess>();
        // The rendered notes, which only make the resampler write the maps, removed with the
        // folder at the end
        QTemporaryDir output(TemporaryStorage::templatePath(QStringLiteral("frequency")));
        if (!output.isValid()) {
            diagnostics.push_back({DiagnosticSeverity::Error,
                                   tr("A temporary folder for the resampler cannot be created.")});
            return 0;
        }
        const fs::path outputDirectory(output.path().toStdU16String());

        QMutex mutex;
        std::atomic<int> done = 0;
        std::atomic<int> generated = 0;
        std::atomic<bool> stopped = false;
        std::atomic<bool> unstarted = false;
        const auto stop = [&] {
            if (!stopped.load() && cancelled && cancelled()) {
                stopped = true;
            }
            return stopped.load();
        };
        {
            QThreadPool pool;
            pool.setMaxThreadCount(threadCount > 0 ? threadCount
                                                   : std::max(1, QThread::idealThreadCount()));
            for (qsizetype i = 0; i < wavs.size(); ++i) {
                pool.start([&, i] {
                    const auto &wav = wavs.at(i);
                    if (!stop() && !format.exists(wav)) {
                        utau::ResamplerArguments arguments;
                        arguments.inFile = wav.u8string();
                        arguments.outFile =
                            (outputDirectory / (std::to_string(i) + ".wav")).u8string();
                        arguments.toneName = "C4";
                        arguments.realLength = 50;
                        DiagnosticList found;
                        const auto run =
                            synthTool->run(resampler, listOf(arguments.arguments()), found, stop);
                        if (!run.started) {
                            // Every other call would fail alike.
                            stopped = true;
                            if (!unstarted.exchange(true)) {
                                found.push_back({DiagnosticSeverity::Error,
                                                 tr("The resampler \"%1\" could not be started.")
                                                     .arg(displayed(resampler))});
                            }
                        } else if (format.exists(wav)) {
                            ++generated;
                        } else if (!run.cancelled) {
                            auto message = tr("The resampler wrote no frequency map of \"%1\" "
                                              "(exit code %2).")
                                               .arg(displayed(wav))
                                               .arg(run.exitCode);
                            if (!run.output.trimmed().isEmpty()) {
                                message += QLatin1Char('\n') + run.output.trimmed();
                            }
                            found.push_back({DiagnosticSeverity::Warning, message});
                        }
                        if (!found.isEmpty()) {
                            const QMutexLocker locker(&mutex);
                            diagnostics += found;
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
        return generated.load();
    }

}
