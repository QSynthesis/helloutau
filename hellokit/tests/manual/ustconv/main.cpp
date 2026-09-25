/// \file
/// Converts between the formats supported by HelloUtau, from the command line.
///
/// Its purpose is integration testing, which the unit tests cannot provide. \c UstDocument,
/// \c Project and \c MidiReader are each tested separately with synthetic inputs, which does
/// not show whether a real UTAU project survives a round trip through all of them. The first
/// roadmap stage is measured by this program, and \c --check performs the measurement.
///
/// \code
///   ustconv song.ust song.usth
///   ustconv --charset Shift_JIS song.usth song.ust
///   ustconv song.mid song.usth
///   ustconv --check song.ust
/// \endcode

#include <cstdio>
#include <filesystem>
#include <string>

#include <QtCore/QByteArrayView>
#include <QtCore/QString>

#include <stdcorelib/console.h>
#include <stdcorelib/path.h>
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

    /// Prints all diagnostics of an operation. None of the output is decorative: a conversion
    /// that loses data reports it, and a run without output lost nothing.
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
            const auto note = diagnostic.noteIndex
                                  ? " (note " + std::to_string(*diagnostic.noteIndex + 1) + ")"
                                  : std::string();
            stdc::console::u8fprintf(stderr, "%s: %s%s\n", level, toStd(diagnostic.message).c_str(),
                                     note.c_str());
        }
    }

    enum class Format {
        Unknown,
        Usth,
        Ust,
        Midi,
    };

    /// The extension is taken as UTF-16, because path::string() converts to the code page of the
    /// system on Windows and fails for a character outside it. Only ASCII letters are folded,
    /// because std::tolower depends on the C locale.
    Format formatOf(const fs::path &path) {
        auto suffix = path.extension().u16string();
        for (auto &c : suffix) {
            if (c >= u'A' && c <= u'Z') {
                c = char16_t(c - u'A' + u'a');
            }
        }
        if (suffix == u".usth") {
            return Format::Usth;
        }
        if (suffix == u".ust") {
            return Format::Ust;
        }
        if (suffix == u".mid" || suffix == u".midi") {
            return Format::Midi;
        }
        return Format::Unknown;
    }

    /// The encoding for reading \a ust , or \c std::nullopt if it cannot be determined.
    ///
    /// If the file does not determine its encoding, the program stops rather than guessing,
    /// because the encoding rules forbid guessing. The user specifies the encoding with
    /// \c --charset .
    std::optional<QString> settleCharset(const UstDocument &ust, const QString &given) {
        if (!given.isEmpty()) {
            return given;
        }
        if (const auto found = ust.settledCharset()) {
            return found;
        }
        stdc::console::u8fputs("error: this UST does not declare its encoding. "
                               "Specify --charset with one of:\n",
                               stderr);
        for (const auto &name : TextCodec::ustCandidates()) {
            stdc::console::u8fprintf(stderr, "  %s\n", toStd(name).c_str());
        }
        return std::nullopt;
    }

    /// Reads \a path in any supported format, without querying the user.
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
        stdc::console::u8fprintf(stderr, "error: %s is not a .usth, a .ust or a .mid\n",
                                 stdc::path::to_utf8(path.filename()).c_str());
        return std::nullopt;
    }

    bool writeAny(const Project &project, const fs::path &path, const QString &charset,
                  DiagnosticList &diagnostics) {
        switch (formatOf(path)) {
            case Format::Usth:
                if (!charset.isEmpty()) {
                    stdc::console::u8fputs("note: a .usth is always UTF-8, so --charset is "
                                           "ignored\n",
                                           stderr);
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
        stdc::console::u8fprintf(stderr, "error: %s is not a .usth, a .ust or a .mid\n",
                                 stdc::path::to_utf8(path.filename()).c_str());
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

    /// Decodes the raw bytes of one file into text.
    ustconv::Normalizer normalizerFor(const UstDocument &ust, const TextCodec &codec) {
        // Escape sequences are meaningful only in a file written with escaping, which the
        // control note identifies. Unescaping any other file would remove its backslashes.
        const bool unescaping = ust.hasControlNote() && !codec.isUtf8();
        return [codec, unescaping](const std::string &bytes) {
            const auto decoded =
                codec.decode(QByteArrayView(bytes.data(), qsizetype(bytes.size())));
            if (!decoded) {
                // Shown verbatim, so that a field invalid in the encoding appears as a
                // difference rather than as a silently empty value on one side.
                return bytes;
            }
            return toStd(unescaping ? TextCodec::unescape(*decoded) : *decoded);
        };
    }

    /// Reads a UST, writes it, reads the result, and reports the differences.
    ///
    /// The file is written and read again rather than compared with the result of
    /// fromProject(), so that writing and parsing are tested in addition to the conversion.
    int check(const fs::path &input, const QString &given) {
        if (formatOf(input) != Format::Ust) {
            stdc::console::u8fputs("error: --check requires a .ust input, because UST is the "
                                   "format whose round trip must be lossless.\n",
                                   stderr);
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

        // The engines are left unset. Substituting local engines applies when a user saves a
        // project, and here it would appear as the round trip introducing settings absent from
        // the file.
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
        stdc::u8printf("wrote %s\n", stdc::path::to_utf8(written).c_str());

        int failures = 0;

        // The encoding must survive as well as the notes. A file without a recorded encoding
        // would require guessing on the next read.
        const auto recorded = after->settledCharset();
        if (recorded != charset) {
            stdc::u8printf("  encoding: %s -> %s\n", toStd(*charset).c_str(),
                           toStd(recorded.value_or(QStringLiteral("nothing"))).c_str());
            ++failures;
        }

        const TextCodec codec(*charset);
        const auto differences =
            ustconv::compare(before->file(), after->file(), normalizerFor(*before, codec),
                             normalizerFor(*after, codec));
        for (const auto &difference : differences) {
            stdc::u8printf("  %s: %s -> %s\n", difference.where.c_str(), difference.before.c_str(),
                           difference.after.c_str());
        }
        failures += int(differences.size());

        if (failures == 0) {
            stdc::u8printf("identical\n");
            return 0;
        }
        stdc::u8printf("%d %s\n", failures, failures == 1 ? "difference" : "differences");
        return 1;
    }

    int run(const stdc::cli::ParseResult &result) {
        const QString charset =
            fromStd(result.valueForOption<std::string>("--charset").value_or(std::string()));
        // The arguments are UTF-8, whereas a path constructed from a narrow string on Windows
        // interprets it in the ANSI code page.
        const auto input = stdc::path::from_utf8(*result.value(0));
        const auto output = result.value(1);

        if (result.option("--check")) {
            if (output) {
                stdc::console::u8fputs("error: --check writes no persistent file and therefore "
                                       "takes no output file\n",
                                       stderr);
                return 1;
            }
            return check(input, charset);
        }

        if (!output) {
            stdc::console::u8fputs("error: no output file. Specify one, or use --check to verify "
                                   "the round trip of this file.\n",
                                   stderr);
            return 1;
        }
        return convert(input, stdc::path::from_utf8(*output), charset);
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
                                   "The encoding of the UST, if the file does not declare one")
                           .arg(cli::Argument("name")))
            .addOption(cli::Option(
                {"--check"},
                "Write the input back and report the differences introduced by the round trip"))
            .setHandler(run)
            .addHelpOption(true)
            .addVersionOption("0.0.1"));

    // Not argv, which on Windows is limited to the ANSI code page and has already lost every
    // unrepresentable character. The wide command line is read instead.
    return parser.invoke(system::command_line_arguments());
}
