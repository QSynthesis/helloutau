#include "UtauScript.h"

#include <fstream>
#include <vector>

#include <QtCore/QFileInfo>
#include <QtCore/QHash>
#include <QtCore/QRegularExpression>

#include <hellokit/Support/TextCodec.h>

namespace utaucompare {

    namespace {

        using hello::kit::TextCodec;

        enum class Engine {
            None,
            Resampler,
            Wavtool,
        };

        std::optional<QString> readText(const std::filesystem::path &path, const TextCodec &codec,
                                        QString *error) {
            std::ifstream file(path, std::ios::binary);
            if (!file) {
                *error =
                    QStringLiteral("cannot open %1").arg(QString::fromStdString(path.string()));
                return std::nullopt;
            }
            const std::string bytes((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
            const auto text = codec.decode(QByteArrayView(bytes.data(), qsizetype(bytes.size())));
            if (!text) {
                *error = QStringLiteral("%1 is not valid %2")
                             .arg(QString::fromStdString(path.string()), codec.name());
                return std::nullopt;
            }
            return text;
        }

        /// Expands the variables in a line as a command processor does, before splitting. The
        /// order matters because \c %params% is one variable containing four arguments, which
        /// become four only after expansion.
        QString expand(const QString &line, const QHash<QString, QString> &variables,
                       const QStringList &positional) {
            QString out;
            out.reserve(line.size());
            for (int i = 0; i < line.size();) {
                if (line.at(i) != QLatin1Char('%')) {
                    out += line.at(i++);
                    continue;
                }
                if (i + 1 < line.size() && line.at(i + 1).isDigit()) {
                    const int which = line.at(i + 1).digitValue();
                    if (which >= 1 && which <= positional.size()) {
                        out += positional.at(which - 1);
                    }
                    i += 2;
                    continue;
                }
                const int close = line.indexOf(QLatin1Char('%'), i + 1);
                if (close < 0) {
                    out += line.at(i++);
                    continue;
                }
                out += variables.value(line.mid(i + 1, close - i - 1));
                i = close + 1;
            }
            return out;
        }

        /// Splits a command line into arguments.
        ///
        /// \param keepQuotes retains the quotes, as required for a positional parameter:
        ///        \c %1 reproduces the token exactly as written, including quotes, and a path
        ///        containing a space remains one argument only because of them.
        QStringList split(const QString &line, bool keepQuotes = false) {
            QStringList out;
            QString current;
            bool quoted = false;
            bool started = false;
            for (const QChar c : line) {
                if (c == QLatin1Char('"')) {
                    quoted = !quoted;
                    started = true;
                    if (keepQuotes) {
                        current += c;
                    }
                    continue;
                }
                if (c.isSpace() && !quoted) {
                    if (started) {
                        out += current;
                        current.clear();
                        started = false;
                    }
                    continue;
                }
                current += c;
                started = true;
            }
            if (started) {
                out += current;
            }
            return out;
        }

        QString unquote(const QString &token) {
            QString out = token;
            out.remove(QLatin1Char('"'));
            return out;
        }

        /// The body of a line, without the leading \c @ and leading whitespace.
        QString body(const QString &line) {
            QString out = line.trimmed();
            while (out.startsWith(QLatin1Char('@'))) {
                out = out.mid(1);
            }
            return out;
        }

    }

    std::optional<QList<ScriptCall>> readScript(const std::filesystem::path &script,
                                                const QString &charset, QString *error) {
        QString ignored;
        if (!error) {
            error = &ignored;
        }

        const TextCodec codec(charset);
        if (!codec.isValid()) {
            *error = QStringLiteral("the encoding %1 is not available").arg(charset);
            return std::nullopt;
        }

        const auto text = readText(script, codec, error);
        if (!text) {
            return std::nullopt;
        }

        // The helper contains the two engine command lines. They are read as templates, so that
        // the argument order is taken from UTAU rather than duplicated here.
        const auto helper = script.parent_path() / "temp_helper.bat";
        if (!std::filesystem::exists(helper)) {
            *error = QStringLiteral("temp_helper.bat is not in the directory of %1. It contains "
                                    "the engine command lines and must be copied as well")
                         .arg(QString::fromStdString(script.string()));
            return std::nullopt;
        }
        const auto helperText = readText(helper, codec, error);
        if (!helperText) {
            return std::nullopt;
        }
        static const QRegularExpression lineBreak(QStringLiteral("\r?\n"));
        const QStringList helperLines = helperText->split(lineBreak);

        static const QRegularExpression assignment(QStringLiteral("^set\\s+([^=\\s]+)=(.*)$"));

        QHash<QString, QString> variables;
        QList<ScriptCall> calls;

        const auto engineOf = [&variables](const QStringList &arguments) {
            if (arguments.isEmpty()) {
                return Engine::None;
            }
            const QString program = QFileInfo(arguments.first()).absoluteFilePath();
            const auto same = [&program, &variables](const char *name) {
                const QString value = split(variables.value(QString::fromLatin1(name))).value(0);
                return !value.isEmpty() && QFileInfo(value).absoluteFilePath() == program;
            };
            if (same("resamp")) {
                return Engine::Resampler;
            }
            if (same("tool")) {
                return Engine::Wavtool;
            }
            return Engine::None;
        };

        const auto runHelper = [&](const QStringList &positional) {
            ScriptCall call;
            bool ok = false;
            const int index = positional.isEmpty() ? 0 : unquote(positional.last()).toInt(&ok);
            if (ok) {
                call.noteIndex = index;
            }
            for (const QString &helperLine : helperLines) {
                const QStringList arguments =
                    split(expand(body(helperLine), variables, positional));
                switch (engineOf(arguments)) {
                    case Engine::Resampler:
                        call.resamplerArguments = arguments.mid(1);
                        break;
                    case Engine::Wavtool:
                        call.wavtoolArguments = arguments.mid(1);
                        break;
                    case Engine::None:
                        break;
                }
            }
            calls += call;
        };

        const QStringList lines = text->split(lineBreak);
        for (const QString &line : lines) {
            const QString statement = body(line);
            if (statement.isEmpty() || statement.startsWith(QStringLiteral("rem"))) {
                continue;
            }

            const auto match = assignment.match(statement);
            if (match.hasMatch()) {
                // Expanded here rather than at the point of use, because a command processor
                // expands it at assignment: UTAU builds the cache path from %cachedir% when it
                // sets the variable. The quotes are retained, because expansion reproduces them.
                variables.insert(match.captured(1),
                                 expand(match.captured(2).trimmed(), variables, QStringList()));
                continue;
            }

            if (statement.startsWith(QStringLiteral("call "))) {
                const QStringList arguments =
                    split(expand(statement.mid(5), variables, QStringList()), true);
                if (!arguments.isEmpty()) {
                    runHelper(arguments.mid(1));
                }
                continue;
            }

            // A note rendered without the helper, as for a rest: no resampler, and one wavtool
            // call that supplies the duration of the silence.
            const QStringList arguments = split(expand(statement, variables, QStringList()));
            if (engineOf(arguments) == Engine::Wavtool) {
                ScriptCall call;
                call.wavtoolArguments = arguments.mid(1);
                calls += call;
            }
        }

        if (calls.isEmpty()) {
            *error = QStringLiteral(
                         "%1 contains no engine calls and is probably not a script written by UTAU")
                         .arg(QString::fromStdString(script.string()));
            return std::nullopt;
        }
        return calls;
    }

}
