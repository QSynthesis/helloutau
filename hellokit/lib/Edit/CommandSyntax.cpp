#include "CommandSyntax.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace hello::kit {

    namespace {

        bool fail(DiagnosticList &diagnostics, const QString &message) {
            Diagnostic diagnostic;
            diagnostic.severity = DiagnosticSeverity::Error;
            diagnostic.message = message;
            diagnostics.push_back(diagnostic);
            return false;
        }

        // Returns whether the backslash at index begins an escape of JSON. The JSON parser of Qt
        // accepts any character after a backslash, which JSON does not, therefore the character
        // is checked here. The parser checks the four digits of the escape of a code unit itself.
        bool isEscape(QStringView line, qsizetype index) {
            return index + 1 < line.size() && QStringView(u"\"\\/bfnrtu").contains(line[index + 1]);
        }

        // Returns the end of the JSON string that begins at start, after its closing quote, or -1
        // if the string is not closed or contains an invalid escape. The escapes are decoded by
        // the JSON parser.
        qsizetype endOfString(QStringView line, qsizetype start) {
            for (qsizetype i = start + 1; i < line.size(); ++i) {
                if (line[i] == QLatin1Char('\\')) {
                    if (!isEscape(line, i)) {
                        return -1;
                    }
                    ++i;
                } else if (line[i] == QLatin1Char('"')) {
                    return i + 1;
                }
            }
            return -1;
        }

        // Returns the end of the JSON object or array that begins at start, after its closing
        // bracket, or -1 if it is not closed. Brackets within strings are not counted.
        qsizetype endOfStructure(QStringView line, qsizetype start) {
            int depth = 0;
            for (qsizetype i = start; i < line.size(); ++i) {
                const auto c = line[i];
                if (c == QLatin1Char('"')) {
                    i = endOfString(line, i);
                    if (i < 0) {
                        return -1;
                    }
                    --i;
                } else if (c == QLatin1Char('{') || c == QLatin1Char('[')) {
                    ++depth;
                } else if (c == QLatin1Char('}') || c == QLatin1Char(']')) {
                    if (--depth == 0) {
                        return i + 1;
                    }
                }
            }
            return -1;
        }

        // Parses text as one JSON value. A string is parsed as the element of an array, because
        // a JSON document is an object or an array.
        std::optional<QJsonValue> parseJson(QStringView text, bool string) {
            const auto document = string ? QLatin1String("[") + text.toString() + QLatin1String("]")
                                         : text.toString();
            QJsonParseError error;
            const auto parsed = QJsonDocument::fromJson(document.toUtf8(), &error);
            if (error.error != QJsonParseError::NoError) {
                return std::nullopt;
            }
            if (string) {
                return parsed.array().at(0);
            }
            return parsed.isObject() ? QJsonValue(parsed.object()) : QJsonValue(parsed.array());
        }

    }

    std::optional<QList<CommandArgument>> CommandSyntax::split(QStringView line,
                                                               DiagnosticList &diagnostics) {
        QList<CommandArgument> arguments;
        qsizetype i = 0;
        while (true) {
            while (i < line.size() && line[i].isSpace()) {
                ++i;
            }
            if (i == line.size()) {
                return arguments;
            }
            if (arguments.isEmpty() && line[i] == QLatin1Char('#')) {
                return arguments;
            }

            const auto c = line[i];
            const auto start = i;
            CommandArgument argument;
            if (c == QLatin1Char('"')) {
                const auto end = endOfString(line, start);
                const auto value =
                    end < 0 ? std::nullopt : parseJson(line.mid(start, end - start), true);
                if (!value) {
                    fail(
                        diagnostics,
                        tr("The string at position %1 is not a valid JSON string.").arg(start + 1));
                    return std::nullopt;
                }
                argument.kind = CommandArgument::String;
                argument.value = *value;
                i = end;
            } else if (c == QLatin1Char('{') || c == QLatin1Char('[')) {
                const auto end = endOfStructure(line, start);
                const auto value =
                    end < 0 ? std::nullopt : parseJson(line.mid(start, end - start), false);
                if (!value) {
                    fail(diagnostics,
                         tr("The value at position %1 is not a valid JSON object or array.")
                             .arg(start + 1));
                    return std::nullopt;
                }
                argument.kind = CommandArgument::Structure;
                argument.value = *value;
                i = end;
            } else {
                while (i < line.size() && !line[i].isSpace()) {
                    if (line[i] == QLatin1Char('"')) {
                        fail(diagnostics, tr("The word at position %1 contains a double quote. "
                                             "Quote the whole argument as a JSON string.")
                                              .arg(start + 1));
                        return std::nullopt;
                    }
                    ++i;
                }
                argument.kind = CommandArgument::Word;
                argument.value = line.mid(start, i - start).toString();
            }

            if (i < line.size() && !line[i].isSpace()) {
                fail(diagnostics,
                     tr("The argument at position %1 is followed by text without whitespace.")
                         .arg(start + 1));
                return std::nullopt;
            }
            arguments.push_back(argument);
        }
    }

}
