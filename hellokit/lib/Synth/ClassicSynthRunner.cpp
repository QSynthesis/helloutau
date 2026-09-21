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

        /// Whether a value can be written into a script at all.
        ///
        /// A quotation mark ends the \c set \c "name=value" form early and a line break ends the
        /// line, and neither has an escape that works inside it. A POSIX shell could carry both,
        /// but the answer is the same on either: a project that renders on one system has to
        /// render on the other, and a limit that moves with the platform is worse than a limit.
        ///
        /// Refusing is the honest answer either way. Writing it mangled would be worse than
        /// saying it cannot be done.
        bool isWritable(const QString &value) {
            for (const QChar c : value) {
                if (c == QLatin1Char('"') || c == QLatin1Char('\n') || c == QLatin1Char('\r')) {
                    return false;
                }
            }
            return true;
        }

        /// How one shell spells the few things a rendering script is made of.
        ///
        /// The layout is UTAU's on both: a header of assignments, a block per note that sets the
        /// note's own values and calls the helper, and a footer that joins the wavtool's two
        /// pieces. Only the spelling differs, which is why this is a table rather than two
        /// copies of the generator.
        class ShellSyntax {
        public:
            using Quoting = ClassicSynthRunner::Quoting;

            ShellSyntax(ClassicSynthRunner::ScriptShell which, Quoting quoting)
                : _batch(which == ClassicSynthRunner::ScriptShell::Batch), _quoting(quoting) {
            }

            bool isBatch() const {
                return _batch;
            }

            const char *scriptName() const {
                return _batch ? "temp.bat" : "temp.sh";
            }

            const char *helperName() const {
                return _batch ? "temp_helper.bat" : "temp_helper.sh";
            }

            /// What every line begins with, which is how a batch file keeps itself quiet.
            const char *quiet() const {
                return _batch ? "@" : "";
            }

            /// Naming a variable that was set earlier.
            QString expand(const char *name) const {
                return _batch ? QLatin1Char('%') + QLatin1String(name) + QLatin1Char('%')
                              : QLatin1String("${") + QLatin1String(name) + QLatin1Char('}');
            }

            /// One assignment, with the value made literal.
            ///
            /// \return nothing where the value cannot be written
            std::optional<QString> assign(const char *name, const QString &value) const {
                const auto key = QLatin1String(name);
                if (_quoting == Quoting::Verbatim) {
                    return _batch ? QLatin1String("@set ") + key + QLatin1Char('=') + value
                                  : QLatin1String("export ") + key + QLatin1Char('=') + value;
                }
                if (!isWritable(value)) {
                    return std::nullopt;
                }
                if (_batch) {
                    // set "name=value" rather than escaping character by character: inside the
                    // quotes cmd stops looking for operators, so &, |, > and ( are all literal.
                    // The per cent sign still has to be doubled, since it is expanded first.
                    QString escaped = value;
                    escaped.replace(QLatin1Char('%'), QLatin1String("%%"));
                    return QLatin1String("@set \"") + key + QLatin1Char('=') + escaped +
                           QLatin1Char('"');
                }
                // Single quotes make a POSIX shell take everything literally, and the one
                // character they cannot hold is the single quote, which leaves and comes back.
                QString escaped = value;
                escaped.replace(QLatin1Char('\''), QLatin1String("'\\''"));
                return QLatin1String("export ") + key + QLatin1String("='") + escaped +
                       QLatin1Char('\'');
            }

            /// One argument as it goes onto a command line in the script.
            ///
            /// Quoted only where it has to be. UTAU quotes its paths and leaves its numbers
            /// bare, and several of these engines read the command line themselves rather than
            /// through a C runtime, so a quoted number is not certain to arrive as a number.
            QString argument(const QString &value) const {
                const QLatin1String quote(_batch ? "\"" : "'");
                if (value.isEmpty()) {
                    return quote + quote;
                }
                const QLatin1String special(_batch ? " \t&|<>^()!,;=" : " \t&|<>^()!;*?$`\"\\");
                for (const QChar c : value) {
                    if (special.toString().contains(c)) {
                        return quote + value + quote;
                    }
                }
                return value;
            }

            /// Removing a file that may not be there.
            QString remove(const QString &path) const {
                return _batch ? QLatin1String("@del \"") + path + QLatin1String("\" 2>nul")
                              : QLatin1String("rm -f \"") + path + QLatin1Char('"');
            }

            QString makeDirectory(const QString &path) const {
                return _batch ? QLatin1String("@mkdir \"") + path + QLatin1String("\" 2>nul")
                              : QLatin1String("mkdir -p \"") + path + QLatin1Char('"');
            }

            QString echo(const QString &text) const {
                return _batch ? QLatin1String("@echo ") + text
                              : QLatin1String("echo '") + text + QLatin1Char('\'');
            }

            /// Running the helper with the note's nine arguments.
            QString callHelper(const QString &arguments) const {
                const auto helper = QLatin1Char('"') + expand("helper") + QLatin1Char('"');
                return _batch ? QLatin1String("@call ") + helper + QLatin1Char(' ') + arguments
                              : helper + QLatin1Char(' ') + arguments;
            }

            /// Running one engine outright, which is what a rest needs.
            QString run(const char *tool, const QString &arguments) const {
                return QLatin1String(quiet()) + QLatin1Char('"') + expand(tool) +
                       QLatin1String("\" ") + arguments;
            }

            /// The lines that start the script, before anything is assigned.
            QStringList prologue() const {
                if (_batch) {
                    return {QLatin1String("@rem hellokit")};
                }
                return {QLatin1String("#!/bin/sh"), QLatin1String("# hellokit")};
            }

            /// Joining the wavtool's header and samples into the track wav, and clearing up.
            QStringList epilogue() const {
                const auto out = expand("output");
                if (_batch) {
                    return {
                        QLatin1String("@if not exist \"") + out + QLatin1String(".whd\" goto E"),
                        QLatin1String("@if not exist \"") + out + QLatin1String(".dat\" goto E"),
                        QLatin1String("@copy /Y \"") + out + QLatin1String(".whd\" /B + \"") + out +
                            QLatin1String(".dat\" /B \"") + out + QLatin1String("\" >nul"),
                        QLatin1String("@del \"") + out + QLatin1String(".whd\""),
                        QLatin1String("@del \"") + out + QLatin1String(".dat\""),
                        QLatin1String(":E"),
                    };
                }
                return {
                    QLatin1String("if [ -f \"") + out + QLatin1String(".whd\" ] && [ -f \"") + out +
                        QLatin1String(".dat\" ]; then"),
                    QLatin1String("\tcat \"") + out + QLatin1String(".whd\" \"") + out +
                        QLatin1String(".dat\" > \"") + out + QLatin1Char('"'),
                    QLatin1String("\trm -f \"") + out + QLatin1String(".whd\""),
                    QLatin1String("\trm -f \"") + out + QLatin1String(".dat\""),
                    QLatin1String("fi"),
                };
            }

            /// The helper, which is the same two calls on either shell with the cache checked
            /// first: a piece that is already there is the cache, and skipping the resampler for
            /// it is how UTAU reuses one.
            QStringList helper() const {
                if (_batch) {
                    return {
                        QLatin1String("@if exist \"%temp%\" goto A"),
                        QLatin1String("@\"%resamp%\" %1 \"%temp%\" %2 %vel% \"%flag%\" %4 %5 %6 "
                                      "%7 %params%"),
                        QLatin1String(":A"),
                        QLatin1String("@\"%tool%\" \"%output%\" \"%temp%\" %stp% %3 %env%"),
                    };
                }
                return {
                    QLatin1String("#!/bin/sh"),
                    QLatin1String("if [ ! -f \"${temp}\" ]; then"),
                    QLatin1String("\t\"${resamp}\" \"$1\" \"${temp}\" $2 ${vel} \"${flag}\" $4 "
                                  "$5 $6 $7 ${params}"),
                    QLatin1String("fi"),
                    QLatin1String("\"${tool}\" \"${output}\" \"${temp}\" ${stp} $3 ${env}"),
                };
            }

            /// The line terminator. A batch file wants CRLF, a shell script does not care and
            /// gets the newline it is usually written with.
            const char *lineEnd() const {
                return _batch ? "\r\n" : "\n";
            }

        private:
            bool _batch;
            Quoting _quoting;
        };

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

        /// Writes the lines a script is made of, and remembers whether a value had to be refused.
        class Writer {
        public:
            Writer(const ShellSyntax &syntax, DiagnosticList &diagnostics)
                : _syntax(syntax), _diagnostics(diagnostics) {
            }

            bool ok() const {
                return _ok;
            }

            const QString &text() const {
                return _text;
            }

            void line(const QString &text) {
                _text += text;
                _text += QLatin1String(_syntax.lineEnd());
            }

            void lines(const QStringList &texts) {
                for (const auto &text : texts) {
                    line(text);
                }
            }

            void set(const char *name, const QString &value, std::optional<int> noteIndex = {}) {
                const auto written = _syntax.assign(name, value);
                if (!written) {
                    fail(_diagnostics,
                         ClassicSynthRunner::tr(
                             "\"%1\" holds a quotation mark or a line break, which a rendering "
                             "script cannot carry.")
                             .arg(QLatin1String(name)),
                         noteIndex);
                    _ok = false;
                    return;
                }
                line(*written);
            }

        private:
            const ShellSyntax &_syntax;
            DiagnosticList &_diagnostics;
            QString _text;
            bool _ok = true;
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

        // The header. UTAU keeps the paths and the values that do not change in variables, and
        // the body then names them; a resampler reading the script finds them where it expects.
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

            // A rest has nothing to resample, so it goes straight to the wavtool, as it does
            // under UTAU.
            if (step.silent) {
                script.line(syntax.run("tool", joined(quotedAll(syntax, step.wavtoolArguments))));
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
                     tr("This note came out with arguments a rendering script cannot be written "
                        "from."),
                     step.noteIndex);
                return std::nullopt;
            }

            script.set("params", joined(r.mid(9)), step.noteIndex);
            script.set("flag", r.at(4), step.noteIndex);
            script.set("env", joined(w.mid(4)), step.noteIndex);
            script.set("stp", w.at(2), step.noteIndex);
            script.set("vel", r.at(3), step.noteIndex);
            script.set("temp", r.at(1), step.noteIndex);
            script.line(syntax.echo(QStringLiteral("(%1/%2)").arg(done).arg(total)));
            script.line(syntax.callHelper(
                joined(quotedAll(syntax, {r.at(0), r.at(2), w.at(3), r.at(5), r.at(6), r.at(7),
                                          r.at(8), QString::number(step.noteIndex)}))));
        }

        // The footer. Nothing has written the track wav yet: the wavtool keeps the header and
        // the samples apart, and joining them is the last thing that happens.
        script.lines(syntax.epilogue());
        helperScript.lines(syntax.helper());

        if (!script.ok() || !helperScript.ok()) {
            return std::nullopt;
        }
        return std::make_pair(script.text(), helperScript.text());
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
            fail(diagnostics,
                 tr("The folder \"%1\" could not be created.").arg(displayed(directory)));
            return outcome;
        }

        const ShellSyntax syntax(shell, quoting);
        const auto scriptPath = directory / syntax.scriptName();
        const auto helperPath = directory / syntax.helperName();

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

        if (!put(scriptPath, written->first) || !put(helperPath, written->second)) {
            fail(diagnostics, tr("The rendering script could not be written to \"%1\".")
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

        const auto run = engine.runScript(scriptPath, diagnostics);

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
                                  ? tr("The rendering script ran but wrote nothing to \"%1\".")
                                        .arg(displayed(plan.outputFile()))
                                  : tr("The rendering script did not run."));
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
