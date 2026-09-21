#include "ClassicSynthRunner.h"

#include <fstream>
#include <system_error>

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>

#include <hellokit/Synth/EngineProcess.h>

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

        /// The two files the wavtool appends to, which the footer joins into the track wav.
        std::pair<fs::path, fs::path> partsOf(const fs::path &output) {
            auto header = output;
            auto data = output;
            header += ".whd";
            data += ".dat";
            return {header, data};
        }

        /// Whether a value can be written into a batch file at all.
        ///
        /// A quotation mark ends the \c set \c "name=value" form early and a newline ends the
        /// line, and neither has an escape that works inside it. Refusing is the honest answer:
        /// writing it mangled would be worse than saying it cannot be done.
        bool isWritable(const QString &value) {
            for (const QChar c : value) {
                if (c == QLatin1Char('"') || c == QLatin1Char('\n') || c == QLatin1Char('\r')) {
                    return false;
                }
            }
            return true;
        }

        QString escaped(const QString &value) {
            // Only the per cent sign. Everything else stays literal inside set "name=value",
            // and delayed expansion is off, so the exclamation mark that a pitch string carries
            // means nothing.
            QString out = value;
            out.replace(QLatin1Char('%'), QLatin1String("%%"));
            return out;
        }

        /// One argument as it goes onto a command line in the script.
        ///
        /// Quoted only where it has to be. UTAU quotes its paths and leaves its numbers bare,
        /// and several of these engines read the command line themselves rather than through a
        /// C runtime, so a quoted number is not certain to arrive as a number.
        QString argument(const QString &value) {
            if (value.isEmpty()) {
                return QLatin1String("\"\"");
            }
            const QLatin1String needsQuotes(" \t&|<>^()!,;=");
            for (const QChar c : value) {
                if (needsQuotes.toString().contains(c)) {
                    return QLatin1Char('"') + value + QLatin1Char('"');
                }
            }
            return value;
        }

        /// The whole vector as one command line, which is what a batch file is made of.
        QString joined(const QStringList &arguments, bool asArguments = false) {
            QString out;
            for (const auto &item : arguments) {
                if (!out.isEmpty()) {
                    out += QLatin1Char(' ');
                }
                out += asArguments ? argument(item) : item;
            }
            return out;
        }

        /// Writes the lines a script is made of, and remembers whether a value had to be refused.
        class Writer {
        public:
            Writer(ClassicSynthRunner::Quoting quoting, DiagnosticList &diagnostics)
                : _quoting(quoting), _diagnostics(diagnostics) {
            }

            bool ok() const {
                return _ok;
            }

            const QString &text() const {
                return _text;
            }

            void line(const QString &text) {
                _text += text;
                _text += QLatin1String("\r\n");
            }

            /// One \c @set , written the way this runner's quoting says.
            void set(const char *name, const QString &value, std::optional<int> noteIndex = {}) {
                const auto key = QLatin1String(name);
                if (_quoting == ClassicSynthRunner::Quoting::Verbatim) {
                    line(QLatin1String("@set ") + key + QLatin1Char('=') + value);
                    return;
                }
                if (!isWritable(value)) {
                    fail(_diagnostics,
                         ClassicSynthRunner::tr(
                             "\"%1\" holds a quotation mark or a line break, which a rendering "
                             "script cannot carry.")
                             .arg(key),
                         noteIndex);
                    _ok = false;
                    return;
                }
                line(QLatin1String("@set \"") + key + QLatin1Char('=') + escaped(value) +
                     QLatin1Char('"'));
            }

        private:
            ClassicSynthRunner::Quoting _quoting;
            DiagnosticList &_diagnostics;
            QString _text;
            bool _ok = true;
        };

    }

    ClassicSynthRunner::ClassicSynthRunner() = default;

    ClassicSynthRunner::~ClassicSynthRunner() = default;

    std::optional<std::pair<QString, QString>>
        ClassicSynthRunner::scripts(const SynthPlan &plan, const SynthEngines &engines,
                                    DiagnosticList &diagnostics) const {
        if (plan.steps().isEmpty()) {
            fail(diagnostics, ClassicSynthRunner::tr("There is nothing to render."));
            return std::nullopt;
        }

        const auto [header, data] = partsOf(plan.outputFile());
        const auto helper =
            (scriptDirectory.empty() ? fs::path() : scriptDirectory) / "temp_helper.bat";

        Writer bat(quoting, diagnostics);
        Writer helperBat(quoting, diagnostics);

        // The header. UTAU keeps the paths and the values that do not change in variables, and
        // the body then names them; a resampler reading the script finds them where it expects.
        bat.line(QLatin1String("@rem hellokit"));
        bat.set("loadmodule", QString());
        bat.set("tool", displayed(engines.wavtool));
        bat.set("resamp", displayed(engines.resampler));
        bat.set("output", displayed(plan.outputFile()));
        bat.set("helper", displayed(helper));
        bat.set("cachedir", displayed(plan.cacheDirectory()));
        bat.line(QLatin1String("@del \"%output%\" 2>nul"));
        bat.line(QLatin1String("@del \"%output%.whd\" 2>nul"));
        bat.line(QLatin1String("@del \"%output%.dat\" 2>nul"));
        bat.line(QLatin1String("@mkdir \"%cachedir%\" 2>nul"));

        const int total = int(plan.steps().size());
        int done = 0;

        for (const auto &step : plan.steps()) {
            ++done;

            // A rest has nothing to resample, so it goes straight to the wavtool, as it does
            // under UTAU.
            if (step.silent) {
                bat.line(QLatin1String("@\"%tool%\" ") + joined(step.wavtoolArguments, true));
                continue;
            }

            // The positions are the engines' own. The resampler reads
            //     <sample> <cache> <tone> <velocity> <flags> <offset> <length> <consonant>
            //     <blank> <intensity> <modulation> <tempo> <pitch>
            // and the wavtool reads
            //     <track> <cache> <stp> <length> <envelope...>
            // which is what SynthPlan already laid out, so the variables below only name the
            // pieces rather than rearranging them.
            const auto &r = step.resamplerArguments;
            const auto &w = step.wavtoolArguments;
            if (r.size() < 9 || w.size() < 4) {
                fail(diagnostics,
                     ClassicSynthRunner::tr("This note came out with arguments a rendering script "
                                            "cannot be written from."),
                     step.noteIndex);
                return std::nullopt;
            }

            bat.set("params", joined(r.mid(9)), step.noteIndex);
            bat.set("flag", r.at(4), step.noteIndex);
            bat.set("env", joined(w.mid(4)), step.noteIndex);
            bat.set("stp", w.at(2), step.noteIndex);
            bat.set("vel", r.at(3), step.noteIndex);
            bat.set("temp", r.at(1), step.noteIndex);
            bat.line(QStringLiteral("@echo (%1/%2)").arg(done).arg(total));
            bat.line(QLatin1String("@call \"%helper%\" ") +
                     joined({r.at(0), r.at(2), w.at(3), r.at(5), r.at(6), r.at(7), r.at(8),
                             QString::number(step.noteIndex)},
                            true));
        }

        // The footer. Nothing has written the track wav yet: the wavtool keeps the header and
        // the samples apart, and joining them is the last thing that happens.
        bat.line(QLatin1String("@if not exist \"%output%.whd\" goto E"));
        bat.line(QLatin1String("@if not exist \"%output%.dat\" goto E"));
        bat.line(QLatin1String("@copy /Y \"%output%.whd\" /B + \"%output%.dat\" /B \"%output%\" "
                               ">nul"));
        bat.line(QLatin1String("@del \"%output%.whd\""));
        bat.line(QLatin1String("@del \"%output%.dat\""));
        bat.line(QLatin1String(":E"));

        // The helper. One note's two calls, with the cache checked first: a piece that is
        // already there is the cache, and skipping the resampler for it is how UTAU reuses one.
        helperBat.line(QLatin1String("@if exist \"%temp%\" goto A"));
        helperBat.line(QLatin1String("@\"%resamp%\" %1 \"%temp%\" %2 %vel% \"%flag%\" %4 %5 %6 "
                                     "%7 %params%"));
        helperBat.line(QLatin1String(":A"));
        helperBat.line(QLatin1String("@\"%tool%\" \"%output%\" \"%temp%\" %stp% %3 %env%"));

        if (!bat.ok() || !helperBat.ok()) {
            return std::nullopt;
        }
        return std::make_pair(bat.text(), helperBat.text());
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

        // Written with the real directory in it, which the caller may have left to us.
        auto self = *this;
        self.scriptDirectory = directory;
        const auto written = self.scripts(plan, engines, diagnostics);
        if (!written) {
            return outcome;
        }

        std::error_code error;
        fs::create_directories(directory, error);
        fs::create_directories(plan.cacheDirectory(), error);
        if (error) {
            fail(diagnostics, ClassicSynthRunner::tr("The folder \"%1\" could not be created.")
                                  .arg(displayed(directory)));
            return outcome;
        }

        const auto batPath = directory / "temp.bat";
        const auto helperPath = directory / "temp_helper.bat";

        const auto put = [&](const fs::path &path, const QString &text) {
            // The code page, not UTF-8. The command processor reads a batch file in the system
            // encoding, and a path this cannot hold is a path the script could not name anyway.
            const auto bytes = text.toLocal8Bit();
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            if (!out) {
                return false;
            }
            out.write(bytes.constData(), bytes.size());
            return bool(out);
        };

        if (!put(batPath, written->first) || !put(helperPath, written->second)) {
            fail(diagnostics,
                 ClassicSynthRunner::tr("The rendering script could not be written to \"%1\".")
                     .arg(displayed(directory)));
            return outcome;
        }

        for (const auto &step : plan.steps()) {
            outcome.silent += step.silent ? 1 : 0;
        }

        if (observer && observer->cancelled()) {
            outcome.cancelled = true;
            return outcome;
        }

        EngineProcess engine;
        engine.timeout = timeout;
        // The script names everything by absolute path, so nothing depends on this. It is set
        // so that an engine writing beside its working directory writes beside the script.
        engine.workingDirectory = directory;

        const auto run = engine.runScript(batPath, diagnostics);

        // One script, so there is one step to report rather than one per note.
        if (observer) {
            observer->progressed(int(plan.steps().size()), int(plan.steps().size()));
        }

        for (const auto &step : plan.steps()) {
            if (step.silent) {
                continue;
            }
            if (fs::exists(step.cacheFile)) {
                ++outcome.resampled;
            } else {
                ++outcome.failed;
            }
        }

        if (!fs::exists(plan.outputFile())) {
            fail(diagnostics, run.started
                                  ? ClassicSynthRunner::tr(
                                        "The rendering script ran but wrote nothing to \"%1\".")
                                        .arg(displayed(plan.outputFile()))
                                  : ClassicSynthRunner::tr("The rendering script did not run."));
            return outcome;
        }

        if (!keepScripts) {
            fs::remove(batPath, error);
            fs::remove(helperPath, error);
            if (scriptDirectory.empty()) {
                fs::remove(directory, error);
            }
        }

        outcome.rendered = true;
        return outcome;
    }

}
