#include "ClassicSynthRunner.h"

#include <fstream>
#include <system_error>

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>

#include "EngineProcess.h"

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

        /// The two files to which the wavtool appends, joined by the footer into the track file.
        std::pair<fs::path, fs::path> partsOf(const fs::path &output) {
            auto header = output;
            auto data = output;
            header += ".whd";
            data += ".dat";
            return {header, data};
        }

        /// Returns whether a value can be written into a script at all.
        ///
        /// A quotation mark terminates the \c set \c "name=value" form prematurely and a line
        /// break terminates the line, and no escape for either works inside that form. A POSIX
        /// shell could represent both, but the result is the same on both shells: a project
        /// that renders on one system must render on the other, and a platform-dependent limit
        /// is worse than a uniform one.
        ///
        /// Rejection is the correct result in either case. Writing a corrupted value would be
        /// worse than reporting that it cannot be written.
        bool isWritable(const QString &value) {
            for (const QChar c : value) {
                if (c == QLatin1Char('"') || c == QLatin1Char('\n') || c == QLatin1Char('\r')) {
                    return false;
                }
            }
            return true;
        }

        /// The syntax of one shell for the constructs a rendering script consists of.
        ///
        /// Both shells use the UTAU layout: a header of assignments, one block per note that
        /// sets the values of the note and calls the helper, and a footer that joins the two
        /// fragments written by the wavtool. Only the syntax differs, which is why this is a
        /// table rather than two copies of the generator.
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

            /// The prefix of every line, which suppresses command echo in a batch file.
            const char *quiet() const {
                return _batch ? "@" : "";
            }

            /// A reference to a previously assigned variable.
            QString expand(const char *name) const {
                return _batch ? QLatin1Char('%') + QLatin1String(name) + QLatin1Char('%')
                              : QLatin1String("${") + QLatin1String(name) + QLatin1Char('}');
            }

            /// One assignment, with the value quoted as a literal.
            ///
            /// \return the assignment, or \c std::nullopt if the value cannot be written
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
                    // The set "name=value" form is used instead of escaping each character,
                    // because cmd does not parse operators inside the quotes, so &, |, > and (
                    // are literal. The percent sign must still be doubled, because variable
                    // expansion precedes parsing.
                    QString escaped = value;
                    escaped.replace(QLatin1Char('%'), QLatin1String("%%"));
                    return QLatin1String("@set \"") + key + QLatin1Char('=') + escaped +
                           QLatin1Char('"');
                }
                // Within single quotes a POSIX shell treats every character literally. The only
                // character that cannot appear inside them is the single quote itself, which is
                // written by closing the quotes, escaping it and reopening them.
                QString escaped = value;
                escaped.replace(QLatin1Char('\''), QLatin1String("'\\''"));
                return QLatin1String("export ") + key + QLatin1String("='") + escaped +
                       QLatin1Char('\'');
            }

            /// One assignment in the form UTAU writes, set name=value without quotes, for a
            /// value that an engine reads from the script text.
            ///
            /// moresampler reads temp.bat to determine whether the current call is the last one
            /// (the Wavtool page cited in docs/Synth.md), so the fragment is written as UTAU
            /// writes it. Outside quotes cmd parses operators, so each of them is escaped with a
            /// caret, which makes the next character literal, and every percent sign is doubled.
            /// A POSIX shell has no such reader, and the value is quoted as by assign().
            ///
            /// \return the assignment, or \c std::nullopt if the value cannot be written
            std::optional<QString> assignUnquoted(const char *name, const QString &value) const {
                if (!_batch || _quoting == Quoting::Verbatim) {
                    return assign(name, value);
                }
                if (!isWritable(value)) {
                    return std::nullopt;
                }
                QString escaped;
                for (const QChar c : value) {
                    if (c == QLatin1Char('%')) {
                        escaped += QLatin1String("%%");
                        continue;
                    }
                    if (QLatin1String("&|<>^()").contains(c)) {
                        escaped += QLatin1Char('^');
                    }
                    escaped += c;
                }
                return QLatin1String("@set ") + QLatin1String(name) + QLatin1Char('=') + escaped;
            }

            /// One argument as written onto a command line in the script.
            ///
            /// Quoted only if necessary. UTAU quotes paths and leaves numbers unquoted, and
            /// several engines parse the command line themselves rather than through a C
            /// runtime, so a quoted number is not guaranteed to be parsed as a number.
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

            /// Deletion of a file that may not exist.
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

            /// Invocation of the helper with the nine arguments of a note.
            QString callHelper(const QString &arguments) const {
                const auto helper = QLatin1Char('"') + expand("helper") + QLatin1Char('"');
                return _batch ? QLatin1String("@call ") + helper + QLatin1Char(' ') + arguments
                              : helper + QLatin1Char(' ') + arguments;
            }

            /// Direct invocation of one engine, as required for a rest.
            QString run(const char *tool, const QString &arguments) const {
                return QLatin1String(quiet()) + QLatin1Char('"') + expand(tool) +
                       QLatin1String("\" ") + arguments;
            }

            /// The opening lines of the script, before any assignment.
            QStringList prologue() const {
                if (_batch) {
                    return {QLatin1String("@rem hellokit")};
                }
                return {QLatin1String("#!/bin/sh"), QLatin1String("# hellokit")};
            }

            /// Joining the header and sample data written by the wavtool into the track file,
            /// followed by cleanup.
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

            /// The helper script, which consists of the same two calls on both shells.
            ///
            /// \param reuse guards the resampler call with a check for an existing fragment,
            ///        which is how the UTAU helper reuses fragments. Without it every note is
            ///        rendered again.
            QStringList helper(bool reuse) const {
                if (_batch) {
                    QStringList lines;
                    if (reuse) {
                        lines += QLatin1String("@if exist \"%temp%\" goto A");
                    }
                    lines += QLatin1String("@\"%resamp%\" %1 \"%temp%\" %2 %vel% \"%flag%\" %4 "
                                           "%5 %6 %7 %params%");
                    if (reuse) {
                        lines += QLatin1String(":A");
                    }
                    lines += QLatin1String("@\"%tool%\" \"%output%\" \"%temp%\" %stp% %3 %env%");
                    return lines;
                }
                QStringList lines = {QLatin1String("#!/bin/sh")};
                const QString call =
                    QLatin1String("\"${resamp}\" \"$1\" \"${temp}\" $2 ${vel} \"${flag}\" $4 "
                                  "$5 $6 $7 ${params}");
                if (reuse) {
                    lines += QLatin1String("if [ ! -f \"${temp}\" ]; then");
                    lines += QLatin1Char('\t') + call;
                    lines += QLatin1String("fi");
                } else {
                    lines += call;
                }
                lines += QLatin1String("\"${tool}\" \"${output}\" \"${temp}\" ${stp} $3 ${env}");
                return lines;
            }

            /// The line terminator: CRLF for a batch file, and LF for a shell script, which
            /// accepts either.
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

        /// Writes the lines of a script and records whether any value was rejected.
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

            void set(const char *name, const QString &value, std::optional<int> noteIndex = {},
                     bool unquoted = false) {
                const auto written =
                    unquoted ? _syntax.assignUnquoted(name, value) : _syntax.assign(name, value);
                if (!written) {
                    fail(_diagnostics,
                         ClassicSynthRunner::tr("\"%1\" contains a quotation mark or a line break, "
                                                "which cannot be written into a rendering script.")
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

            // A rest requires no resampling and is passed directly to the wavtool, as in UTAU.
            if (step.silent) {
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
            script.line(syntax.echo(QStringLiteral("(%1/%2)").arg(done).arg(total)));
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

        // Written with the actual directory, which the caller may have left unspecified.
        auto self = *this;
        self.scriptDirectory = directory;
        const auto written = self.scripts(plan, engines, diagnostics);
        if (!written) {
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
                alreadyThere[i] = !step.silent && fs::exists(step.cacheFile);
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

        const auto put = [&](const fs::path &path, const QString &text) {
            // The ANSI code page, not UTF-8. The command processor reads a batch file in the
            // system encoding, and a path unrepresentable in it could not be referenced by the
            // script in any case.
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

        const auto engine = makeEngineProcess();
        // The script references everything by absolute path, so nothing depends on this. It is
        // set so that an engine writing to its working directory writes beside the script.
        engine->workingDirectory = directory;

        const auto run = engine->runScript(scriptPath, diagnostics);

        // A single script, so progress is reported as one step rather than one per note.
        if (observer) {
            observer->progressed(int(plan.steps().size()), int(plan.steps().size()));
        }

        for (int i = 0; i < int(plan.steps().size()); ++i) {
            const auto &step = plan.steps().at(i);
            if (step.silent) {
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
