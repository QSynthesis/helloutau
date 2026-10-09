#include "ClassicSynthRunner.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <optional>
#include <system_error>
#include <vector>

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>

#include <hellokit/Support/TextCodec.h>

#include "EngineProcess.h"
#include "ShellSyntax_p.h"

namespace hello::kit {

    namespace fs = std::filesystem;

    namespace {

        void fail(DiagnosticList &diagnostics, const QString &message,
                  std::optional<int> noteIndex = std::nullopt) {
            diagnostics.push_back({DiagnosticSeverity::Error, message, noteIndex});
        }

        QString displayed(const fs::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

        // Returns the first character of text that codec cannot represent. A surrogate pair counts
        // as one character.
        QString firstUnrepresentable(const TextCodec &codec, const QString &text) {
            for (qsizetype i = 0; i < text.size();) {
                const qsizetype length =
                    text.at(i).isHighSurrogate() && i + 1 < text.size() ? 2 : 1;
                const auto character = text.mid(i, length);
                if (!codec.canEncode(character)) {
                    return character;
                }
                i += length;
            }
            return {};
        }

        /// The two files to which the wavtool appends, joined by the footer into the track file.
        std::pair<fs::path, fs::path> partsOf(const fs::path &output) {
            auto header = output;
            auto data = output;
            header += ".whd";
            data += ".dat";
            return {header, data};
        }

        QString joined(const QStringList &items) {
            QString out;
            for (const auto &item : items) {
                if (!out.isEmpty()) {
                    out += QLatin1Char(' ');
                }
                out += item;
            }
            return out;
        }

        QStringList quotedAll(const ShellSyntax &syntax, const QStringList &items) {
            QStringList out;
            out.reserve(items.size());
            for (const auto &item : items) {
                out.push_back(syntax.argument(item));
            }
            return out;
        }

        /// Writes the lines of a script and records whether any value was rejected.
        class Writer {
        public:
            Writer(const ShellSyntax &syntax, DiagnosticList &diagnostics)
                : m_syntax(syntax), m_diagnostics(diagnostics) {
            }

            bool ok() const {
                return m_ok;
            }

            const QString &text() const {
                return m_text;
            }

            void line(const QString &text) {
                m_text += text;
                m_text += QLatin1String(m_syntax.lineEnd());
            }

            void lines(const QStringList &texts) {
                for (const auto &text : texts) {
                    line(text);
                }
            }

            void set(const char *name, const QString &value, std::optional<int> noteIndex = {},
                     bool unquoted = false) {
                const auto written =
                    unquoted ? m_syntax.assignUnquoted(name, value) : m_syntax.assign(name, value);
                if (!written) {
                    fail(m_diagnostics,
                         ClassicSynthRunner::tr("\"%1\" contains a quotation mark or a line break, "
                                                "which cannot be written into a rendering script.")
                             .arg(QLatin1String(name)),
                         noteIndex);
                    m_ok = false;
                    return;
                }
                line(*written);
            }

        private:
            const ShellSyntax &m_syntax;
            DiagnosticList &m_diagnostics;
            QString m_text;
            bool m_ok = true;
        };

    }

    ClassicSynthRunner::ClassicSynthRunner() = default;

    ClassicSynthRunner::~ClassicSynthRunner() = default;

    ClassicSynthRunner::ScriptShell ClassicSynthRunner::nativeShell() {
#ifdef _WIN32
        return ScriptShell::Batch;
#else
        return ScriptShell::Posix;
#endif
    }

    std::optional<std::pair<QString, QString>>
        ClassicSynthRunner::scripts(const SynthPlan &plan, const SynthEngines &engines,
                                    DiagnosticList &diagnostics) const {
        if (plan.steps().isEmpty()) {
            fail(diagnostics, tr("There is nothing to render."));
            return std::nullopt;
        }

        const ShellSyntax syntax(shell, quoting);
        const auto helper =
            (scriptDirectory.empty() ? fs::path() : scriptDirectory) / syntax.helperName();

        Writer script(syntax, diagnostics);
        Writer helperScript(syntax, diagnostics);

        // The header. UTAU stores the paths and the constant values in variables, which the
        // body then references. A resampler that reads the script finds them at the usual location.
        script.lines(syntax.prologue());
        script.set("loadmodule", QString());
        script.set("tool", displayed(engines.wavtool));
        script.set("resamp", displayed(engines.resampler));
        script.set("output", displayed(plan.outputFile()));
        script.set("helper", displayed(helper));
        script.set("cachedir", displayed(plan.cacheDirectory()));

        const auto output = syntax.expand("output");
        script.line(syntax.remove(output));
        script.line(syntax.remove(output + QLatin1String(".whd")));
        script.line(syntax.remove(output + QLatin1String(".dat")));
        script.line(syntax.makeDirectory(syntax.expand("cachedir")));

        const int total = int(plan.steps().size());
        int done = 0;

        for (const auto &step : plan.steps()) {
            ++done;

            // A rest, and a note with $patch or $direct, requires no resampling and is passed
            // directly to the wavtool, as in UTAU.
            if (!step.resamples()) {
                // The file of $patch is named by the project. A value that cannot be quoted is
                // refused rather than written.
                for (const auto &argument : step.wavtoolArguments) {
                    if (!isWritable(argument)) {
                        fail(diagnostics,
                             tr("The arguments of this note cannot be written into a rendering "
                                "script."),
                             step.noteIndex);
                        return std::nullopt;
                    }
                }
                script.line(syntax.run("tool", joined(quotedAll(syntax, step.wavtoolArguments))));
                continue;
            }

            // The positions are the engines' own. The resampler reads
            //     <sample> <cache> <tone> <velocity> <flags> <offset> <length> <consonant>
            //     <blank> <intensity> <modulation> <tempo> <pitch>
            // and the wavtool reads
            //     <track> <cache> <stp> <length> <envelope...>
            // which is the order SynthPlan already produces, so the variables below only name
            // the arguments rather than rearranging them.
            const auto &r = step.resamplerArguments;
            const auto &w = step.wavtoolArguments;
            if (r.size() < 9 || w.size() < 4) {
                fail(diagnostics,
                     tr("The arguments of this note cannot be written into a rendering script."),
                     step.noteIndex);
                return std::nullopt;
            }

            script.set("params", joined(r.mid(9)), step.noteIndex);
            script.set("flag", r.at(4), step.noteIndex);
            script.set("env", joined(w.mid(4)), step.noteIndex);
            script.set("stp", w.at(2), step.noteIndex);
            script.set("vel", r.at(3), step.noteIndex);
            // As UTAU writes it, for the engines that read the script
            script.set("temp", r.at(1), step.noteIndex, true);
            script.line(syntax.echo(ShellSyntax::progress(done, total)));
            script.line(syntax.callHelper(
                joined(quotedAll(syntax, {r.at(0), r.at(2), w.at(3), r.at(5), r.at(6), r.at(7),
                                          r.at(8), QString::number(step.noteIndex)}))));
        }

        // The footer. The track file does not exist yet, because the wavtool writes the header
        // and the sample data separately, and joining them is the final step.
        script.lines(syntax.epilogue());
        helperScript.lines(syntax.helper(reuseCache));

        if (!script.ok() || !helperScript.ok()) {
            return std::nullopt;
        }
        return std::make_pair(script.text(), helperScript.text());
    }

    std::optional<QList<ClassicSynthRunner::ScriptFile>>
        ClassicSynthRunner::scriptFiles(const fs::path &directory, const SynthPlan &plan,
                                        const SynthEngines &engines,
                                        DiagnosticList &diagnostics) const {
        // The scripts refer to the helper by its path in the directory.
        auto self = *this;
        self.scriptDirectory = directory;
        const ShellSyntax syntax(shell, quoting);
        const auto written = self.scripts(plan, engines, diagnostics);
        if (!written) {
            return std::nullopt;
        }
        QList<std::pair<fs::path, QString>> texts{
            {directory / syntax.scriptName(), written->first },
            {directory / syntax.helperName(), written->second},
        };
#ifndef _WIN32
        // An engine run through Wine, such as moresampler, reads temp.bat in its working
        // directory, which is this directory.
        auto batchSelf = self;
        batchSelf.shell = ScriptShell::Batch;
        const auto batch = batchSelf.scripts(plan, engines, diagnostics);
        if (!batch) {
            return std::nullopt;
        }
        texts.push_back({directory / "temp.bat", batch->first});
        texts.push_back({directory / "temp_helper.bat", batch->second});
#endif

        // The system code page, in which the command processor reads a batch file. A script
        // with a character outside it is refused. UTAU writes a question mark in its place, which
        // makes the file name invalid and the note silent without a report.
        const TextCodec codec;
        QList<ScriptFile> files;
        for (const auto &[path, text] : texts) {
            if (!codec.canEncode(text)) {
                fail(diagnostics, tr("The rendering script cannot contain \"%1\", which the "
                                     "system code page cannot represent.")
                                      .arg(firstUnrepresentable(codec, text)));
                return std::nullopt;
            }
            files.push_back({path, codec.encode(text)});
        }
        return files;
    }

    bool ClassicSynthRunner::writeScriptFiles(const QList<ScriptFile> &files) {
        for (const auto &[path, bytes] : files) {
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            if (!out) {
                return false;
            }
            out.write(bytes.constData(), bytes.size());
            if (!out) {
                return false;
            }
        }
        return true;
    }

    SynthOutcome ClassicSynthRunner::render(const SynthPlan &plan, const SynthEngines &engines,
                                            SynthObserver *observer,
                                            DiagnosticList &diagnostics) const {
        SynthOutcome outcome;

        auto directory = scriptDirectory;
        if (directory.empty()) {
            std::error_code error;
            directory =
                fs::temp_directory_path(error) /
                ("hellokit-" + QString::number(QDateTime::currentMSecsSinceEpoch()).toStdString());
        }

        const auto files = scriptFiles(directory, plan, engines, diagnostics);
        if (!files) {
            return outcome;
        }

        std::error_code error;
        fs::create_directories(directory, error);
        fs::create_directories(plan.cacheDirectory(), error);
        forgetSuperseded(plan, diagnostics);

        // The fragments the script will find already present. The script does not report what
        // it skips, so this is the only place where reuse can be counted.
        QList<bool> alreadyThere(int(plan.steps().size()), false);
        if (reuseCache) {
            for (int i = 0; i < int(plan.steps().size()); ++i) {
                const auto &step = plan.steps().at(i);
                alreadyThere[i] = step.resamples() && fs::exists(step.cacheFile);
            }
        }
        if (error) {
            fail(diagnostics,
                 tr("The folder \"%1\" could not be created.").arg(displayed(directory)));
            return outcome;
        }

        const ShellSyntax syntax(shell, quoting);
        const auto scriptPath = directory / syntax.scriptName();
        const auto helperPath = directory / syntax.helperName();

        if (!writeScriptFiles(*files)) {
            fail(diagnostics, tr("The rendering script could not be written to \"%1\".")
                                  .arg(displayed(directory)));
            return outcome;
        }

        for (const auto &step : plan.steps()) {
            outcome.silent += step.silent ? 1 : 0;
            outcome.direct += step.direct ? 1 : 0;
        }

        if (observer && observer->cancelled()) {
            outcome.cancelled = true;
            return outcome;
        }

        // The script removes the track file first as well, but a script that does not start
        // would leave the file of the previous render, to be taken for the result.
        fs::remove(plan.outputFile(), error);

        const auto engine = makeEngineProcess();
        // The script references everything by absolute path, so nothing depends on this. It is
        // set so that an engine writing to its working directory writes beside the script.
        engine->workingDirectory = directory;
        // The limit of one engine call for each call the script makes, a resampler and a
        // wavtool call per note, rather than one call's limit for the whole track.
        engine->timeout =
            int(std::min<qint64>(std::numeric_limits<int>::max(),
                                 qint64(engine->timeout) * 2 * qint64(plan.steps().size())));

        // The write time of each fragment before the script runs, or nothing if the fragment does
        // not exist yet
        std::vector<std::optional<fs::file_time_type>> writtenBefore(plan.steps().size());
        for (int i = 0; i < int(plan.steps().size()); ++i) {
            std::error_code timeError;
            const auto written = fs::last_write_time(plan.steps().at(i).cacheFile, timeError);
            if (plan.steps().at(i).resamples() && !timeError) {
                writtenBefore[size_t(i)] = written;
            }
        }

        const auto run = engine->runScript(
            scriptPath, diagnostics, [observer] { return observer && observer->cancelled(); });

        // The script was killed during one of its engine calls, and the resampler of that call
        // may have written part of its fragment. The script runs the notes in track order. That
        // fragment is therefore the last fragment that this run created or rewrote, and it is
        // removed. If the kill occurred during a wavtool call or before the resampler created its
        // file, the removed fragment is complete, and the next render resamples one note again.
        if (run.cancelled || run.timedOut) {
            for (int i = int(plan.steps().size()) - 1; i >= 0; --i) {
                const auto &step = plan.steps().at(i);
                std::error_code timeError;
                const auto written = fs::last_write_time(step.cacheFile, timeError);
                if (!step.resamples() || timeError || writtenBefore[size_t(i)] == written) {
                    continue;
                }
                fs::remove(step.cacheFile, error);
                break;
            }
        }

        if (run.cancelled) {
            outcome.cancelled = true;
            return outcome;
        }

        // A single script, so progress is reported as one step rather than one per note.
        if (observer) {
            observer->progressed(int(plan.steps().size()), int(plan.steps().size()));
        }

        for (int i = 0; i < int(plan.steps().size()); ++i) {
            const auto &step = plan.steps().at(i);
            if (!step.resamples()) {
                continue;
            }
            if (alreadyThere.at(i)) {
                ++outcome.reused;
            } else if (fs::exists(step.cacheFile)) {
                ++outcome.resampled;
            } else {
                ++outcome.failed;
            }
        }

        if (!fs::exists(plan.outputFile())) {
            fail(diagnostics, run.started
                                  ? tr("The rendering script ran but produced no output at \"%1\".")
                                        .arg(displayed(plan.outputFile()))
                                  : tr("The rendering script could not be run."));
            return outcome;
        }

        if (!keepScripts) {
            fs::remove(scriptPath, error);
            fs::remove(helperPath, error);
            if (scriptDirectory.empty()) {
                fs::remove(directory, error);
            }
        }

        outcome.rendered = true;
        return outcome;
    }

}
