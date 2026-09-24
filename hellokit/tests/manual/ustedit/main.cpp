/// \file
/// Edits a project with commands, from the command line.
///
/// Its purpose is integration testing of the editing layer without an interface: a real project
/// is opened, a sequence of commands is executed, undone and redone, and the result is saved for
/// UTAU to open. See the acceptance criteria in docs/Editing.md.
///
/// \code
///   ustedit song.usth --script edits.txt -o out.usth
///   ustedit song.ust --charset Shift_JIS -o out.ust < edits.txt
///   ustedit song.usth --script edits.txt --dump-changes
/// \endcode
///
/// Each line of the script is a command of ProjectCommands, or one of the following, which
/// operate on the undo history rather than on the project:
///
/// \code
///   undo [<count>|all]
///   redo [<count>|all]
/// \endcode
///
/// The script is read as UTF-8, with or without a byte order mark. The first command that fails
/// stops the program, and no file is written.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <QtCore/QByteArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QString>

#include <stdcorelib/support/commandline.h>
#include <stdcorelib/system.h>

#include <hellokit/Document/UstDocument.h>
#include <hellokit/EditBase/CommandSyntax.h>
#include <hellokit/Edit/ProjectCommands.h>
#include <hellokit/Edit/ProjectSession.h>
#include <hellokit/Support/TextCodec.h>

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
            std::cerr << level << ": ";
            if (line > 0) {
                std::cerr << "line " << line << ": ";
            }
            std::cerr << toStd(diagnostic.message);
            if (diagnostic.noteIndex) {
                std::cerr << " (note " << (*diagnostic.noteIndex + 1) << ")";
            }
            std::cerr << std::endl;
        }
    }

    bool hasExtension(const fs::path &path, const char *extension) {
        auto suffix = path.extension().string();
        for (auto &c : suffix) {
            c = char(std::tolower(static_cast<unsigned char>(c)));
        }
        return suffix == extension;
    }

    /// Reads \a path as a \c .usth or a \c .ust, without querying the user. A UST that does not
    /// declare its encoding requires \a charset, because the encoding rules forbid guessing.
    std::optional<Project> read(const fs::path &path, const QString &charset) {
        DiagnosticList diagnostics;
        std::optional<Project> project;
        if (hasExtension(path, ".usth")) {
            project = Project::open(path, diagnostics);
        } else if (hasExtension(path, ".ust")) {
            const auto ust = UstDocument::open(path, diagnostics);
            const auto settled = !ust                ? std::nullopt
                                 : charset.isEmpty() ? ust->settledCharset()
                                                     : std::optional<QString>(charset);
            if (ust && !settled) {
                std::cerr << "error: this UST does not declare its encoding. Specify --charset "
                             "with one of:"
                          << std::endl;
                for (const auto &name : TextCodec::ustCandidates()) {
                    std::cerr << "  " << toStd(name) << std::endl;
                }
            }
            if (settled) {
                project = ust->toProject(*settled, diagnostics);
            }
        } else {
            std::cerr << "error: " << path.filename().string() << " is not a .usth or a .ust"
                      << std::endl;
        }
        report(diagnostics);
        return project;
    }

    bool write(const Project &project, const fs::path &path, const QString &charset) {
        DiagnosticList diagnostics;
        bool written = false;
        if (hasExtension(path, ".usth")) {
            written = project.save(path, diagnostics);
        } else if (hasExtension(path, ".ust")) {
            UstDocument::ExportOptions options;
            if (!charset.isEmpty()) {
                options.charset = charset;
            }
            const auto ust = UstDocument::fromProject(project, options, diagnostics);
            written = ust && ust->save(path, diagnostics);
        } else {
            std::cerr << "error: " << path.filename().string() << " is not a .usth or a .ust"
                      << std::endl;
        }
        report(diagnostics);
        return written;
    }

    /// Executes \c undo or \c redo with \a arguments, the arguments after the name, and returns
    /// whether the arguments are valid. Undoing more steps than the history holds stops at its
    /// start, as \c all does.
    bool travel(ProjectSession &session, bool undo, const QList<edit::CommandArgument> &arguments,
                DiagnosticList &diagnostics) {
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
    bool execute(ProjectSession &session, const QString &line, DiagnosticList &diagnostics) {
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
        }
        return ProjectCommands::execute(session, line, diagnostics);
    }

    /// Executes the lines of \a in, and returns whether every line succeeded.
    bool runScript(ProjectSession &session, std::istream &in) {
        std::string bytes;
        for (int number = 1; std::getline(in, bytes); ++number) {
            if (!bytes.empty() && bytes.back() == '\r') {
                bytes.pop_back();
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
        const fs::path input = *result.value(0);
        const QString charset =
            fromStd(result.valueForOption<std::string>("--charset").value_or(std::string()));
        const auto script = result.valueForOption<std::string>("--script");
        const auto output = result.valueForOption<std::string>("--output");

        const auto project = read(input, charset);
        if (!project) {
            return 1;
        }

        ProjectSession session(*project);
        if (result.option("--dump-changes")) {
            QObject::connect(&session, &edit::EditSession::changed, &session,
                             [&session](const edit::ChangePtr &change) {
                                 if (const auto entry = session.logEntry(*change)) {
                                     const auto line =
                                         QJsonDocument(*entry).toJson(QJsonDocument::Compact);
                                     std::cout.write(line.constData(), line.size());
                                     std::cout << '\n';
                                 }
                             });
        }

        bool succeeded = false;
        if (script) {
            std::ifstream in(fs::u8path(*script), std::ios::binary);
            if (!in) {
                std::cerr << "error: the script could not be opened" << std::endl;
                return 1;
            }
            succeeded = runScript(session, in);
        } else {
            succeeded = runScript(session, std::cin);
        }
        std::cout.flush();
        if (!succeeded) {
            return 1;
        }

        if (output) {
            const auto path = fs::u8path(*output);
            return write(session.snapshot(), path, charset) ? 0 : 1;
        }
        return 0;
    }

}

int main(int argc, char *argv[]) {
    Q_UNUSED(argc)
    Q_UNUSED(argv)

    using namespace stdc;

    cli::Parser parser(
        cli::Command("ustedit", "Edit a .usth or a .ust with commands")
            .addArgument(cli::Argument("input", "The project to edit"))
            .addOption(cli::Option({"-s", "--script"},
                                   "The file of commands, one per line. Standard input if absent")
                           .arg(cli::Argument("file")))
            .addOption(cli::Option({"-o", "--output"},
                                   "The file to write the result to. Nothing is written if absent")
                           .arg(cli::Argument("file")))
            .addOption(cli::Option({"-c", "--charset"},
                                   "The encoding of the UST, if the file does not declare one")
                           .arg(cli::Argument("name")))
            .addOption(cli::Option({"--dump-changes"},
                                   "Print each change as a line of JSON to standard output"))
            .setHandler(run)
            .addHelpOption(true)
            .addVersionOption("0.0.1"));

    // Not argv, which on Windows is limited to the ANSI code page and has already lost every
    // unrepresentable character. The wide command line is read instead.
    return parser.invoke(system::command_line_arguments());
}
