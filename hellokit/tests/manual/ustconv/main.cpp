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

#include <QtCore/QByteArrayView>
#include <QtCore/QString>

#include <stdcorelib/support/commandline.h>
#include <stdcorelib/system.h>

#include <hellokit/Document/UstDocument.h>
#include <hellokit/Interchange/Formats/MidiConvert.h>
#include <hellokit/Support/TextCodec.h>

#include "UstCompare.h"

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

    /// The encoding to read \a ust with, or nothing where nobody can say.
    ///
    /// Where the file itself does not settle it this stops rather than guessing, since guessing
    /// is the one thing the encoding rules forbid. The caller says which with \c --charset .
    std::optional<QString> settleCharset(const UstDocument &ust, const QString &given) {
        if (!given.isEmpty()) {
            return given;
        }
        if (const auto found = ust.settledCharset()) {
            return found;
        }
        std::cerr << "error: this UST does not say what encoding it is in. "
                     "Pass --charset with one of:"
                  << std::endl;
        for (const auto &name : TextCodec::ustCandidates()) {
            std::cerr << "  " << toStd(name) << std::endl;
        }
        return std::nullopt;
    }

    /// Reads whatever \a path is, asking for nothing.
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
                const auto settled = settleCharset(*ust, charset);
                if (!settled) {
                    return std::nullopt;
                }
                return ust->toProject(*settled, diagnostics);
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
        std::cerr << "error: " << path.filename().string() << " is not a .usth, a .ust or a .mid"
                  << std::endl;
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
        std::cerr << "error: " << path.filename().string() << " is not a .usth, a .ust or a .mid"
                  << std::endl;
        return false;
    }

    int convert(const fs::path &input, const fs::path &output, const QString &charset) {
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

    /// Reads the raw bytes of one file as the text they stand for.
    ustconv::Normalizer normalizerFor(const UstDocument &ust, const TextCodec &codec) {
        // An escape means what it says only in a file written with escapes, and the control note
        // is what says so. Unescaping anything else would eat its backslashes.
        const bool unescaping = ust.hasControlNote() && !codec.isUtf8();
        return [codec, unescaping](const std::string &bytes) {
            const auto decoded =
                codec.decode(QByteArrayView(bytes.data(), qsizetype(bytes.size())));
            if (!decoded) {
                // Shown as it stands, so that a field the encoding cannot read is visible in the
                // difference rather than silently empty on one side.
                return bytes;
            }
            return toStd(unescaping ? TextCodec::unescape(*decoded) : *decoded);
        };
    }

    /// Reads a UST, writes it back out, reads that, and says what the trip changed.
    ///
    /// The file is written and read again rather than compared against what fromProject() built,
    /// so that writing and parsing are both measured and not only the conversion between them.
    int check(const fs::path &input, const QString &given) {
        if (formatOf(input) != Format::Ust) {
            std::cerr << "error: --check reads a .ust. UST is the format that has to keep "
                         "everything, so it is the one worth measuring."
                      << std::endl;
            return 1;
        }

        DiagnosticList diagnostics;
        const auto before = UstDocument::open(input, diagnostics);
        report(diagnostics);
        if (!before) {
            return 1;
        }

        const auto charset = settleCharset(*before, given);
        if (!charset) {
            return 1;
        }

        diagnostics.clear();
        const auto project = before->toProject(*charset, diagnostics);
        report(diagnostics);
        if (!project) {
            return 1;
        }

        // The engines are left unset. Naming a local one is for a user saving a project, and
        // here it would read as the round trip inventing settings that were never in the file.
        UstDocument::ExportOptions options;
        options.charset = *charset;

        auto name = input.stem();
        name += ".check.ust";
        const auto written = fs::temp_directory_path() / name;

        diagnostics.clear();
        const auto out = UstDocument::fromProject(*project, options, diagnostics);
        if (!out || !out->save(written, diagnostics)) {
            report(diagnostics);
            return 1;
        }
        report(diagnostics);

        diagnostics.clear();
        const auto after = UstDocument::open(written, diagnostics);
        report(diagnostics);
        if (!after) {
            return 1;
        }
        std::cout << "wrote " << written.string() << std::endl;

        int failures = 0;

        // The encoding has to survive as well as the notes. A file that comes back saying
        // nothing about what it is in would have to be guessed at next time.
        const auto recorded = after->settledCharset();
        if (recorded != charset) {
            std::cout << "  encoding: " << toStd(*charset) << " -> "
                      << toStd(recorded.value_or(QStringLiteral("nothing"))) << std::endl;
            ++failures;
        }

        const TextCodec codec(*charset);
        const auto differences =
            ustconv::compare(before->file(), after->file(), normalizerFor(*before, codec),
                             normalizerFor(*after, codec));
        for (const auto &difference : differences) {
            std::cout << "  " << difference.where << ": " << difference.before << " -> "
                      << difference.after << std::endl;
        }
        failures += int(differences.size());

        if (failures == 0) {
            std::cout << "identical" << std::endl;
            return 0;
        }
        std::cout << failures << (failures == 1 ? " difference" : " differences") << std::endl;
        return 1;
    }

    int run(const stdc::cli::ParseResult &result) {
        const QString charset =
            fromStd(result.valueForOption<std::string>("--charset").value_or(std::string()));
        const fs::path input = *result.value(0);
        const auto output = result.value(1);

        if (result.option("--check")) {
            if (output) {
                std::cerr << "error: --check writes nothing anyone keeps, so it takes no output "
                             "file"
                          << std::endl;
                return 1;
            }
            return check(input, charset);
        }

        if (!output) {
            std::cerr << "error: no output file. Pass one, or --check to read back what writing "
                         "this file would produce."
                      << std::endl;
            return 1;
        }
        return convert(input, fs::path(*output), charset);
    }

}

int main(int argc, char *argv[]) {
    Q_UNUSED(argc)
    Q_UNUSED(argv)

    using namespace stdc;

    cli::Parser parser(
        cli::Command("ustconv", "Convert between .usth, .ust and MIDI")
            .addArgument(cli::Argument("input", "The file to read"))
            .addArgument(cli::Argument("output", "The file to write, unless --check").optional())
            .addOption(cli::Option({"-c", "--charset"},
                                   "The encoding of the UST, where the file does not say")
                           .arg(cli::Argument("name")))
            .addOption(cli::Option({"--check"},
                                   "Write the input back out and report what the trip changed"))
            .setHandler(run)
            .addHelpOption(true)
            .addVersionOption("0.0.1"));

    // Not argv, which on Windows is whatever the ANSI code page could hold and has already lost
    // anything it could not. This reads the wide command line again.
    return parser.invoke(system::command_line_arguments());
}
