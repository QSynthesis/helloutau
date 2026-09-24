#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtTest/QTest>

#include <hellokit/Edit/CommandSyntax.h>

using namespace hello::kit;

// The command lines are ordinary string literals rather than raw string literals, because moc
// does not recognize the class of a file whose raw strings contain unpaired quotes.
class test_CommandSyntax : public QObject {
    Q_OBJECT

private:
    static QList<CommandArgument> split(const QString &line) {
        DiagnosticList diagnostics;
        const auto arguments = CommandSyntax::split(line, diagnostics);
        if (!arguments) {
            return {};
        }
        return *arguments;
    }

    static CommandArgument word(const QString &text) {
        return {CommandArgument::Word, text};
    }

    static CommandArgument string(const QString &text) {
        return {CommandArgument::String, text};
    }

    static CommandArgument structure(const QJsonValue &value) {
        return {CommandArgument::Structure, value};
    }

private Q_SLOTS:
    void words_are_separated_by_whitespace() {
        QCOMPARE(
            split(QStringLiteral("  note  set\t0 12 lyric a ")),
            QList<CommandArgument>({word(QStringLiteral("note")), word(QStringLiteral("set")),
                                    word(QStringLiteral("0")), word(QStringLiteral("12")),
                                    word(QStringLiteral("lyric")), word(QStringLiteral("a"))}));
    }

    // A string is a JSON string, with every escape of JSON.
    void a_string_is_decoded_as_json() {
        // The last argument is the escape of U+3042, assembled so that the source contains no
        // backslash followed by u.
        const auto line = QStringLiteral("set \"a i\" \"say \\\"hi\\\"\" \"back\\\\slash\" "
                                         "\"line\\nbreak\" \"\\") +
                          QStringLiteral("u3042\"");
        const auto arguments = split(line);
        QCOMPARE(arguments.size(), 6);
        QCOMPARE(arguments[1], string(QStringLiteral("a i")));
        QCOMPARE(arguments[2], string(QStringLiteral("say \"hi\"")));
        QCOMPARE(arguments[3], string(QStringLiteral("back\\slash")));
        QCOMPARE(arguments[4], string(QStringLiteral("line\nbreak")));
        QCOMPARE(arguments[5], string(QString(QChar(0x3042))));
        QCOMPARE(split(QStringLiteral("\"\"")), QList<CommandArgument>({string(QString())}));
    }

    // A verbatim string has no escapes, so that a Windows path is written as it is. Two double
    // quotes denote one.
    void a_verbatim_string_is_taken_as_written() {
        const auto arguments = split(QStringLiteral(
            "set @\"C:\\tools\\resampler.exe\" @\"say \"\"hi\"\"\" @\"\" @\"a b\" @\"\"\"\""));
        QCOMPARE(arguments.size(), 6);
        QCOMPARE(arguments[1], string(QStringLiteral("C:\\tools\\resampler.exe")));
        QCOMPARE(arguments[2], string(QStringLiteral("say \"hi\"")));
        QCOMPARE(arguments[3], string(QString()));
        QCOMPARE(arguments[4], string(QStringLiteral("a b")));
        QCOMPARE(arguments[5], string(QStringLiteral("\"")));
    }

    // Only an at sign followed by a double quote begins a verbatim string.
    void an_at_sign_alone_begins_a_word() {
        QCOMPARE(split(QStringLiteral("@ @a a@")),
                 QList<CommandArgument>({word(QStringLiteral("@")), word(QStringLiteral("@a")),
                                         word(QStringLiteral("a@"))}));
    }

    // A word reads as a JSON literal if it is one, whatever field receives it.
    void a_word_is_a_json_literal_or_text() {
        const QList<std::pair<QString, QJsonValue>> words{
            {QStringLiteral("12"),       12                        },
            {QStringLiteral("-1.5e2"),   -150                      },
            {QStringLiteral("true"),     true                      },
            {QStringLiteral("false"),    false                     },
            {QStringLiteral("null"),     QJsonValue::Null          },
            {QStringLiteral("a"),        QStringLiteral("a")       },
            {QStringLiteral("012"),      QStringLiteral("012")     },
            {QStringLiteral(".5"),       QStringLiteral(".5")      },
            {QStringLiteral("1,2"),      QStringLiteral("1,2")     },
            {QStringLiteral("NaN"),      QStringLiteral("NaN")     },
            {QStringLiteral("Infinity"), QStringLiteral("Infinity")},
            {QStringLiteral("/a/0"),     QStringLiteral("/a/0")    },
        };
        for (const auto &[text, value] : words) {
            QVERIFY2(CommandSyntax::valueOf(word(text)) == value, qPrintable(text));
        }
    }

    // Quoting makes text of a word that reads as a literal.
    void a_string_and_a_structure_are_their_values() {
        QCOMPARE(CommandSyntax::valueOf(string(QStringLiteral("12"))),
                 QJsonValue(QStringLiteral("12")));
        QCOMPARE(CommandSyntax::valueOf(string(QStringLiteral("null"))),
                 QJsonValue(QStringLiteral("null")));
        QCOMPARE(CommandSyntax::valueOf(structure(QJsonArray{1})), QJsonValue(QJsonArray{1}));
    }

    // A structure is written as in a .usth file, with whitespace, quotes and brackets inside,
    // including brackets within its strings.
    void a_structure_is_read_to_its_matching_bracket() {
        const auto arguments = split(
            QStringLiteral("set /v {\"period\": 180, \"label\": \"a } b\", \"list\": [1, [2]]} "
                           "[1, \"]\"] tail"));
        QCOMPARE(arguments.size(), 5);

        QJsonObject object;
        object.insert(QStringLiteral("period"), 180);
        object.insert(QStringLiteral("label"), QStringLiteral("a } b"));
        object.insert(QStringLiteral("list"), QJsonArray{1, QJsonArray{2}});
        QCOMPARE(arguments[2], structure(object));
        QCOMPARE(arguments[3], structure(QJsonArray{1, QStringLiteral("]")}));
        QCOMPARE(arguments[4], word(QStringLiteral("tail")));
    }

    void empty_lines_and_comments_contain_no_command() {
        DiagnosticList diagnostics;
        for (const auto &line : {QString(), QStringLiteral("   "), QStringLiteral("# note"),
                                 QStringLiteral("  #note set")}) {
            const auto arguments = CommandSyntax::split(line, diagnostics);
            QVERIFY(arguments.has_value());
            QVERIFY(arguments->isEmpty());
        }
        QVERIFY(diagnostics.isEmpty());

        // A # after the first argument is part of a word.
        QCOMPARE(split(QStringLiteral("set #1")),
                 QList<CommandArgument>({word(QStringLiteral("set")), word(QStringLiteral("#1"))}));
    }

    void malformed_lines_are_refused() {
        const QStringList lines{
            QStringLiteral("set \"unterminated"),
            QStringLiteral("set \"bad \\x escape\""),
            QStringLiteral("set {\"a\": 1"),
            QStringLiteral("set {\"a\": } "),
            QStringLiteral("set \"a\"b"),
            QStringLiteral("set {\"a\": 1}b"),
            QStringLiteral("set a\"b\""),
            QStringLiteral("set \"\\") + QStringLiteral("u12G4\""),
            QStringLiteral("set {\"a\": \"\\x\"}"),
            QStringLiteral("set @\"unterminated"),
            QStringLiteral("set @\"a\"\""),
            QStringLiteral("set @\"a\"b"),
        };
        for (const auto &line : lines) {
            DiagnosticList diagnostics;
            QVERIFY2(!CommandSyntax::split(line, diagnostics).has_value(), qPrintable(line));
            QCOMPARE(diagnostics.size(), 1);
            QVERIFY(hasError(diagnostics));
        }
    }
};

QTEST_APPLESS_MAIN(test_CommandSyntax)

#include "test_CommandSyntax.moc"
