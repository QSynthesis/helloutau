/// \file
/// Writes a project designed to determine UTAU behavior.
///
/// In a probe, each note varies one property and keeps all others fixed. Rendering it in UTAU
/// and keeping the resulting \c temp.bat yields a table that maps each entry to the argument
/// UTAU passed. Nearly all known facts about the UTAU arguments were determined this way.
///
/// The manifest written beside the project records the question of each note, so that the
/// answers can be evaluated by a program rather than by inspection.
///
/// \code
///   utauprobe arguments probe.ust --voice "New Geping UTAU Database" --charset GBK \
///       --out E:/compare/probe-utau.wav --cache probe.cache --flags B0
/// \endcode
///
/// \sa utaucompare, which evaluates the answers after UTAU has rendered the probe

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <QtCore/QString>

#include <stdcorelib/support/commandline.h>
#include <stdcorelib/system.h>

#include <hellokit/Support/TextCodec.h>

#include "Probe.h"

using namespace hello::kit;
using namespace utauprobe;
namespace fs = std::filesystem;

namespace {

    std::string toStd(const QString &text) {
        return text.toStdString();
    }

    QString fromStd(const std::string &text) {
        return QString::fromStdString(text);
    }

    std::string option(const stdc::cli::ParseResult &result, const char *token) {
        return result.valueForOption<std::string>(token).value_or(std::string());
    }

    /// A path from the command line.
    ///
    /// The arguments are UTF-8, whereas on Windows a \c fs::path constructed from a narrow
    /// string interprets it in the ANSI code page. Any non-ASCII path would then refer to a
    /// nonexistent file, which affects most voice banks and many projects.
    fs::path pathOf(const std::string &text) {
        return fs::u8path(text);
    }

    bool write(const fs::path &path, const QByteArray &bytes) {
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            std::cerr << "error: cannot write " << path.string() << std::endl;
            return false;
        }
        file.write(bytes.constData(), bytes.size());
        return bool(file);
    }

    int run(const stdc::cli::ParseResult &result) {
        const QString kind = fromStd(*result.value(0));
        const fs::path output = pathOf(*result.value(1));

        Probe probe;
        if (kind == QStringLiteral("arguments")) {
            probe = argumentProbe();
        } else if (kind == QStringLiteral("vibrato")) {
            probe = vibratoProbe();
        } else {
            std::cerr << "error: there is no probe called " << toStd(kind)
                      << ". There is arguments and there is vibrato" << std::endl;
            return 1;
        }

        Probe::Settings settings;
        settings.name = fromStd(output.stem().string());
        // The UTAU placeholder for its voice bank directory.
        settings.voiceDir = QStringLiteral("%VOICE%") + fromStd(option(result, "--voice"));
        settings.outFile = fromStd(option(result, "--out"));
        settings.cacheDir = fromStd(option(result, "--cache"));
        settings.flags = fromStd(option(result, "--flags"));
        const auto tempo = option(result, "--tempo");
        if (!tempo.empty()) {
            settings.tempo = fromStd(tempo);
        }

        // UTAU reads a UST in the ANSI code page of the host, so the probe is written in the
        // encoding in which it will be opened, not in UTF-8.
        const TextCodec codec(fromStd(option(result, "--charset")));
        if (!codec.isValid()) {
            std::cerr << "error: there is no encoding called " << option(result, "--charset")
                      << std::endl;
            return 1;
        }

        if (!write(output, codec.encode(probe.toUst(settings)))) {
            return 1;
        }
        const fs::path manifest = fs::path(output).replace_extension(".manifest.tsv");
        if (!write(manifest, probe.toManifest().toUtf8())) {
            return 1;
        }

        int sung = 0;
        int ticks = 0;
        for (const auto &note : probe.notes()) {
            sung += note.lyric == QStringLiteral("R") ? 0 : 1;
            ticks += note.length;
        }
        std::cout << "wrote " << output.string() << ": " << probe.notes().size() << " notes, "
                  << sung << " of them sung" << std::endl;
        std::cout << "      " << manifest.string() << std::endl;
        std::cout << "total " << ticks << " ticks, "
                  << (double(ticks) / 480.0 * 60.0 / settings.tempo.toDouble()) << " seconds at "
                  << toStd(settings.tempo) << " bpm" << std::endl;
        return 0;
    }

}

int main(int argc, char *argv[]) {
    Q_UNUSED(argc)
    Q_UNUSED(argv)

    using namespace stdc;

    cli::Parser parser(
        cli::Command("utauprobe", "Write a project designed to determine UTAU behavior")
            .addArgument(cli::Argument("kind", "arguments, or vibrato"))
            .addArgument(cli::Argument("output", "The .ust to write"))
            .addOption(cli::Option({"--voice"},
                                   "The voice bank folder in UTAU notation: the folder name "
                                   "only, relative to the voice directory of UTAU")
                           .arg(cli::Argument("name")))
            .addOption(
                cli::Option({"--out"}, "The WAV file UTAU renders to").arg(cli::Argument("path")))
            .addOption(
                cli::Option({"--cache"}, "The cache directory for UTAU").arg(cli::Argument("path")))
            .addOption(cli::Option({"--flags"}, "The project flags").arg(cli::Argument("flags")))
            .addOption(cli::Option({"--tempo"}, "The project tempo, in UTAU notation")
                           .arg(cli::Argument("bpm")))
            .addOption(cli::Option({"-c", "--charset"},
                                   "The encoding of the UST, which must be the ANSI code page of "
                                   "the machine running UTAU. Defaults to that of this machine")
                           .arg(cli::Argument("name")))
            .setHandler(run)
            .addHelpOption(true)
            .addVersionOption("0.0.1"));

    return parser.invoke(system::command_line_arguments());
}
