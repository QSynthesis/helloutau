/// \file
/// Converts between the formats HelloUTAU reads and writes, from a terminal.
///
/// It exists to put the pieces together, which their own tests cannot do. Each of \c UstDocument,
/// \c Project and \c MidiReader is covered on its own with inputs written for it, and none of
/// that says whether a real UTAU project survives a trip through all of them. This is what the
/// roadmap's first stage is measured by, and \c --check is the measurement.
///
/// \code
///   ustconv song.ust song.usth
///   ustconv --charset Shift_JIS song.usth song.ust
///   ustconv song.mid song.usth
///   ustconv --check song.ust
/// \endcode

#include <filesystem>
#include <iostream>
#include <string>

#include <QtCore/QString>

#include <stdcorelib/support/commandline.h>
#include <stdcorelib/system.h>

#include <hellokit/Document/UstDocument.h>
#include <hellokit/Interchange/Formats/MidiConvert.h>
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

    /// Prints everything an operation had to say. Nothing here is decoration: a conversion that
    /// lost something says so, and a run that prints nothing lost nothing.
    void report(const DiagnosticList &diagnostics) {
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
            std::cerr << level << ": " << toStd(diagnostic.message);
            if (diagnostic.noteIndex) {
                std::cerr << " (note " << (*diagnostic.noteIndex + 1) << ")";
            }
            std::cerr << std::endl;
        }
    }

    enum class Format {
        Unknown,
        Usth,
        Ust,
        Midi,
    };

    Format formatOf(const fs::path &path) {
        auto suffix = path.extension().string();
        for (auto &c : suffix) {
            c = char(std::tolower(static_cast<unsigned char>(c)));
        }
        if (suffix == ".usth") {
            return Format::Usth;
        }
        if (suffix == ".ust") {
            return Format::Ust;
        }
        if (suffix == ".mid" || suffix == ".midi") {
            return Format::Midi;
        }
        return Format::Unknown;
    }

    /// Reads whatever \a path is, asking for nothing.
    ///
    /// Where the encoding of a UST cannot be settled from the file itself this stops rather than
    /// guessing, since guessing is the one thing the encoding rules forbid. The caller says which
    /// encoding with \c --charset .
    std::optional<Project> readAny(const fs::path &path, const QString &charset,
                                   DiagnosticList &diagnostics) {
        switch (formatOf(path)) {
            case Format::Usth:
                return Project::open(path, diagnostics);

            case Format::Ust: {
                const auto ust = UstDocument::open(path, diagnostics);
                if (!ust) {
                    return std::nullopt;
                }
                QString settled = charset;
                if (settled.isEmpty()) {
                    const auto found = ust->settledCharset();
                    if (!found) {
                        std::cerr << "error: this UST does not say what encoding it is in. "
                                     "Pass --charset with one of:" << std::endl;
                        for (const auto &name : TextCodec::ustCandidates()) {
                            std::cerr << "  " << toStd(name) << std::endl;
                        }
                        return std::nullopt;
                    }
                    settled = *found;
                }
                return ust->toProject(settled, diagnostics);
            }

            case Format::Midi: {
                MidiReader reader;
                auto result = reader.read(path, nullptr);
                diagnostics.append(result.diagnostics);
                return result.project;
            }

            case Format::Unknown:
                break;
        }
        std::cerr << "error: " << path.filename().string()
                  << " is not a .usth, a .ust or a .mid" << std::endl;
        return std::nullopt;
    }

    bool writeAny(const Project &project, const fs::path &path, const QString &charset,
                  DiagnosticList &diagnostics) {
        switch (formatOf(path)) {
            case Format::Usth:
                if (!charset.isEmpty()) {
                    std::cerr << "note: a .usth is always UTF-8, so --charset is ignored"
                              << std::endl;
                }
                return project.save(path, diagnostics);

            case Format::Ust: {
                UstDocument::ExportOptions options;
                if (!charset.isEmpty()) {
                    options.charset = charset;
                }
                const auto ust = UstDocument::fromProject(project, options, diagnostics);
                return ust && ust->save(path, diagnostics);
            }

            case Format::Midi: {
                MidiWriter writer;
                auto result = writer.write(project, path, nullptr);
                diagnostics.append(result.diagnostics);
                return result.written;
            }

            case Format::Unknown:
                break;
        }
        std::cerr << "error: " << path.filename().string()
                  << " is not a .usth, a .ust or a .mid" << std::endl;
        return false;
    }

    int convert(const stdc::cli::ParseResult &result) {
        const fs::path input = *result.value(0);
        const fs::path output = *result.value(1);
        const QString charset = fromStd(result.valueForOption<std::string>("--charset")
                                            .value_or(std::string()));

        DiagnosticList diagnostics;
        const auto project = readAny(input, charset, diagnostics);
        report(diagnostics);
        if (!project) {
            return 1;
        }

        diagnostics.clear();
        const bool written = writeAny(*project, output, charset, diagnostics);
        report(diagnostics);
        return written ? 0 : 1;
    }

}

int main(int argc, char *argv[]) {
    Q_UNUSED(argc)
    Q_UNUSED(argv)

    using namespace stdc;

    cli::Parser parser(
        cli::Command("ustconv", "Convert between .usth, .ust and MIDI")
            .addArgument(cli::Argument("input", "The file to read"))
            .addArgument(cli::Argument("output", "The file to write"))
            .addOption(cli::Option({"-c", "--charset"},
                                   "The encoding of the UST, where the file does not say")
                           .arg(cli::Argument("name")))
            .setHandler(convert)
            .addHelpOption(true)
            .addVersionOption("0.0.1"));

    // Not argv, which on Windows is whatever the ANSI code page could hold and has already lost
    // anything it could not. This reads the wide command line again.
    return parser.invoke(system::command_line_arguments());
}
