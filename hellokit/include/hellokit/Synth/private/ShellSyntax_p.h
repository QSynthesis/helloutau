#ifndef HELLOKIT_SYNTH_SHELLSYNTAX_P_H
#define HELLOKIT_SYNTH_SHELLSYNTAX_P_H

#include <optional>

#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Synth/ClassicSynthRunner.h>

namespace hello::kit {

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
    inline bool isWritable(const QString &value) {
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
        ///
        /// A percent sign is doubled in a batch file, inside quotes as well, because variable
        /// expansion precedes parsing. A single quote is written in a shell script by closing
        /// the quotes, escaping it and reopening them. A quotation mark or a line break cannot
        /// be written in a batch file, which isWritable() reports and the caller checks.
        QString argument(const QString &value) const {
            const QLatin1String quote(_batch ? "\"" : "'");
            if (value.isEmpty()) {
                return quote + quote;
            }
            QString text = value;
            if (_batch) {
                text.replace(QLatin1Char('%'), QLatin1String("%%"));
            } else {
                text.replace(QLatin1Char('\''), QLatin1String("'\\''"));
            }
            const QLatin1String special(_batch ? " \t&|<>^()!,;=" : " \t&|<>^()!;*?$`\"\\'");
            for (const QChar c : value) {
                if (special.toString().contains(c)) {
                    return quote + text + quote;
                }
            }
            return text;
        }

        /// A value in quotes whatever it contains, as a path in a test or a copy is written:
        /// with every percent sign doubled in a batch file, and every single quote closed,
        /// escaped and reopened in a shell script. A quotation mark or a line break cannot be
        /// written in a batch file, which isWritable() reports and the caller checks.
        QString quoted(const QString &value) const {
            QString text = value;
            if (_batch) {
                text.replace(QLatin1Char('%'), QLatin1String("%%"));
                return QLatin1Char('"') + text + QLatin1Char('"');
            }
            text.replace(QLatin1Char('\''), QLatin1String("'\\''"));
            return QLatin1Char('\'') + text + QLatin1Char('\'');
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

        /// The progress that UTAU shows before note \a done of \a total, counted from 1: a bar
        /// of 40 characters, \c # for the part done, rounded with halves to even, then \c - ,
        /// and the count. See "与 UTAU 的实测对照" in docs/Synth.md.
        static QString progress(int done, int total) {
            constexpr int width = 40;
            const qint64 scaled = qint64(width) * done;
            qint64 filled = scaled / total;
            const qint64 remainder = scaled % total;
            if (2 * remainder > total || (2 * remainder == total && filled % 2 == 1)) {
                ++filled;
            }
            return QString(qsizetype(filled), QLatin1Char('#')) +
                   QString(qsizetype(width - filled), QLatin1Char('-')) +
                   QStringLiteral("(%1/%2)").arg(done).arg(total);
        }

        /// Invocation of the helper with the nine arguments of a note.
        QString callHelper(const QString &arguments) const {
            const auto helper = QLatin1Char('"') + expand("helper") + QLatin1Char('"');
            return _batch ? QLatin1String("@call ") + helper + QLatin1Char(' ') + arguments
                          : helper + QLatin1Char(' ') + arguments;
        }

        /// Direct invocation of one engine, as required for a rest.
        QString run(const char *tool, const QString &arguments) const {
            return QLatin1String(quiet()) + QLatin1Char('"') + expand(tool) + QLatin1String("\" ") +
                   arguments;
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

}

#endif // HELLOKIT_SYNTH_SHELLSYNTAX_P_H
