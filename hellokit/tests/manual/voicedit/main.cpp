/// \file
/// Edits a voice bank with commands, from the command line.
///
/// Its purpose is integration testing of the editing layer on a real voice bank without an
/// interface: the voice bank is opened, a sequence of commands is executed, undone and redone,
/// and the result is saved into the voice bank. See the acceptance criteria in docs/Editing.md.
///
/// \code
///   voicedit path/to/bank --charset GBK --script edits.txt --save
///   voicedit path/to/bank --charset Shift_JIS --dump-changes < edits.txt
///   voicedit path/to/bank --charset GBK --script edits.txt --save-as path/to/copy [--text-only]
/// \endcode
///
/// Each line of the script is a command or a query of VoiceBankCommands, or one of the following,
/// which operate on the undo history rather than on the voice bank:
///
/// \code
///   undo [<count>|all]
///   redo [<count>|all]
/// \endcode
///
/// A query, such as <tt>get /directories/1/otoEntries</tt>, prints its result as one line of JSON
/// to standard output.
///
/// The script is read as UTF-8, with or without a byte order mark. The first command that fails
/// stops the program, and no file is written.
///
/// \warning Saving writes into the voice bank itself. Edit a copy, or save it as a new folder,
///          which leaves the original unchanged.

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <QtCore/QByteArray>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QString>

#include <stdcorelib/console.h>
#include <stdcorelib/support/commandline.h>
#include <stdcorelib/system.h>

#include <hellokit/EditBase/CommandSyntax.h>
#include <hellokit/Edit/VoiceBankCommands.h>
#include <hellokit/Edit/VoiceBankSession.h>
#include <hellokit/VoiceBank/VoiceBankFileSystemState.h>

using namespace hello::kit;
namespace fs = std::filesystem;

namespace {

    std::string toStd(const QString &text) {
        return text.toStdString();
    }

    QString fromStd(const std::string &text) {
        return QString::fromStdString(text);
    }

    /// Prints all diagnostics of an operation, with the line of the script if \a line is
    /// positive.
    void report(const DiagnosticList &diagnostics, int line = 0) {
        for (const auto &diagnostic : diagnostics) {
            const char *level = "note";
            switch (diagnostic.severity) {
                case DiagnosticSeverity::Warning:
                    level = "warning";
                    break;
                case DiagnosticSeverity::Error:
                    level = "error";
                    break;
                case DiagnosticSeverity::Note:
                    break;
            }
            const auto where = line > 0 ? "line " + std::to_string(line) + ": " : std::string();
            stdc::console::u8fprintf(stderr, "%s: %s%s\n", level, where.c_str(),
                                     toStd(diagnostic.message).c_str());
        }
    }

    /// Prints \a value as one line of JSON to standard output.
    void print(const QJsonValue &value) {
        // QJsonDocument holds an object or an array only, so a value is written as the one
        // element of an array, whose brackets are then removed.
        const auto line = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
        stdc::u8printf("%.*s\n", int(line.size() - 2), line.constData() + 1);
    }

    /// Executes \c undo or \c redo with \a arguments, the arguments after the name, and returns
    /// whether the arguments are valid. Undoing more steps than the history holds stops at its
    /// start, as \c all does.
    bool travel(edit::EditSession &session, bool undo,
                const QList<edit::CommandArgument> &arguments, DiagnosticList &diagnostics) {
        int count = 1;
        if (arguments.size() == 1 && arguments[0].text() == QLatin1String("all")) {
            count = -1;
        } else if (arguments.size() == 1) {
            const auto value = edit::CommandSyntax::valueOf(arguments[0]);
            count = value.isDouble() ? value.toInt(-1) : -1;
            if (count < 1 || double(count) != value.toDouble()) {
                diagnostics.push_back(
                    {DiagnosticSeverity::Error,
                     QStringLiteral("The count of %1 must be a positive integer or all.")
                         .arg(undo ? QStringLiteral("undo") : QStringLiteral("redo")),
                     std::nullopt});
                return false;
            }
        } else if (!arguments.isEmpty()) {
            diagnostics.push_back({DiagnosticSeverity::Error,
                                   QStringLiteral("Usage: %1 [<count>|all]")
                                       .arg(undo ? QStringLiteral("undo") : QStringLiteral("redo")),
                                   std::nullopt});
            return false;
        }
        for (; count != 0 && (undo ? session.canUndo() : session.canRedo()); --count) {
            undo ? session.undo() : session.redo();
        }
        return true;
    }

    /// Executes \a line, and returns whether it succeeded.
    bool execute(VoiceBankSession &session, const QString &line, DiagnosticList &diagnostics) {
        const auto arguments = edit::CommandSyntax::split(line, diagnostics);
        if (!arguments) {
            return false;
        }
        if (!arguments->isEmpty() && arguments->first().kind == edit::CommandArgument::Word) {
            const auto name = arguments->first().text();
            if (name == QLatin1String("undo") || name == QLatin1String("redo")) {
                return travel(session, name == QLatin1String("undo"), arguments->mid(1),
                              diagnostics);
            }
            if (VoiceBankCommands::queryNames().contains(name)) {
                const auto result = VoiceBankCommands::query(session, line, diagnostics);
                if (result) {
                    print(*result);
                }
                return result.has_value();
            }
        }
        return VoiceBankCommands::execute(session, line, diagnostics);
    }

    /// Executes the lines of \a in, and returns whether every line succeeded.
    bool runScript(VoiceBankSession &session, std::istream &in) {
        std::string bytes;
        for (int number = 1; std::getline(in, bytes); ++number) {
            if (!bytes.empty() && bytes.back() == '\r') {
                bytes.pop_back();
            }
            // A byte order mark begins the first line of a script saved by some editors.
            if (number == 1 && bytes.rfind("\xEF\xBB\xBF", 0) == 0) {
                bytes.erase(0, 3);
            }
            DiagnosticList diagnostics;
            const bool succeeded = execute(session, QString::fromUtf8(bytes), diagnostics);
            report(diagnostics, number);
            if (!succeeded) {
                return false;
            }
        }
        return true;
    }

    int run(const stdc::cli::ParseResult &result) {
        const auto root = fs::u8path(*result.value(0));
        const QString charset =
            fromStd(result.valueForOption<std::string>("--charset").value_or(std::string()));
        const auto script = result.valueForOption<std::string>("--script");
        const auto saveAs = result.valueForOption<std::string>("--save-as");
        if (saveAs && result.option("--save")) {
            stdc::console::u8fputs("error: --save and --save-as exclude each other\n", stderr);
            return 1;
        }
        if (result.option("--text-only") && !saveAs) {
            stdc::console::u8fputs("error: --text-only requires --save-as\n", stderr);
            return 1;
        }

        // Every directory whose encoding is not recorded is read in the given encoding, or left
        // out without one, rather than decoded by guesswork.
        FixedCharsetSelector selector(charset);
        DiagnosticList diagnostics;
        auto opened = VoiceBankFileSystemState::open(root, charset.isEmpty() ? nullptr : &selector,
                                                     diagnostics);
        if (!opened) {
            report(diagnostics);
            return 1;
        }
        auto session = VoiceBankSession::create(std::move(*opened), diagnostics);
        report(diagnostics);
        if (!session) {
            return 1;
        }
        for (const auto &directory : session->excludedDirectories()) {
            stdc::console::u8fprintf(
                stderr, "note: \"%s\" is not edited, because it %s\n",
                toStd(QString::fromStdU16String(directory.path.generic_u16string())).c_str(),
                directory.leftOut ? "was not read" : "did not decode");
        }

        if (result.option("--dump-changes")) {
            QObject::connect(session.get(), &edit::EditSession::changed, session.get(),
                             [&session](const edit::ChangePtr &change) {
                                 if (const auto entry = session->logEntry(*change)) {
                                     const auto line =
                                         QJsonDocument(*entry).toJson(QJsonDocument::Compact);
                                     stdc::u8printf("%.*s\n", int(line.size()), line.constData());
                                 }
                             });
        }

        bool succeeded = false;
        if (script) {
            std::ifstream in(fs::u8path(*script), std::ios::binary);
            if (!in) {
                stdc::console::u8fputs("error: the script could not be opened\n", stderr);
                return 1;
            }
            succeeded = runScript(*session, in);
        } else {
            succeeded = runScript(*session, std::cin);
        }
        std::fflush(stdout);
        if (!succeeded) {
            return 1;
        }

        if (saveAs) {
            DiagnosticList saving;
            const bool saved =
                session->saveAs(fs::u8path(*saveAs),
                                result.option("--text-only") ? VoiceBankSession::TextFiles
                                                             : VoiceBankSession::AllFiles,
                                saving);
            report(saving);
            return saved ? 0 : 1;
        }
        if (result.option("--save")) {
            DiagnosticList saving;
            const bool saved = session->save(saving);
            report(saving);
            return saved ? 0 : 1;
        }
        return 0;
    }

}

int main(int argc, char *argv[]) {
    Q_UNUSED(argc)
    Q_UNUSED(argv)

    using namespace stdc;

    cli::Parser parser(
        cli::Command("voicedit", "Edit a voice bank with commands")
            .addArgument(cli::Argument("folder", "The voice bank to edit"))
            .addOption(cli::Option({"-s", "--script"},
                                   "The file of commands, one per line. Standard input if absent")
                           .arg(cli::Argument("file")))
            .addOption(cli::Option({"-c", "--charset"},
                                   "The encoding of each directory whose encoding is not recorded")
                           .arg(cli::Argument("name")))
            .addOption(cli::Option({"--save"},
                                   "Save the result into the voice bank. Nothing is written if "
                                   "absent"))
            .addOption(cli::Option({"--save-as"},
                                   "Save the result into a new folder, which must not exist or "
                                   "be empty, copying every other file of the voice bank")
                           .arg(cli::Argument("folder")))
            .addOption(cli::Option({"--text-only"},
                                   "With --save-as, write the text files only and copy nothing"))
            .addOption(cli::Option({"--dump-changes"},
                                   "Print each change as a line of JSON to standard output"))
            .setHandler(run)
            .addHelpOption(true)
            .addVersionOption("0.0.1"));

    // Not argv, which on Windows is limited to the ANSI code page and has already lost every
    // unrepresentable character. The wide command line is read instead.
    return parser.invoke(system::command_line_arguments());
}
