/// \file
/// Writes a project built to ask UTAU questions.
///
/// A probe is a project where every note varies one thing and holds the rest still. Render it in
/// UTAU, keep the \c temp.bat it wrote, and the script becomes a table of "given this entry, it
/// passed that argument". Nearly everything settled about UTAU's arguments was settled this way.
///
/// The manifest that goes out beside the project says which note asks what, so that reading the
/// answers back is something a program can do rather than something done by eye.
///
/// \code
///   utauprobe arguments probe.ust --voice "New Geping UTAU Database" --charset GBK \
///       --out E:/compare/probe-utau.wav --cache probe.cache --flags B0
/// \endcode
///
/// \sa utaucompare, which is what reads the answers once UTAU has rendered it

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
        const fs::path output = *result.value(1);

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
        // UTAU's own placeholder for wherever it keeps its voice banks.
        settings.voiceDir = QStringLiteral("%VOICE%") + fromStd(option(result, "--voice"));
        settings.outFile = fromStd(option(result, "--out"));
        settings.cacheDir = fromStd(option(result, "--cache"));
        settings.flags = fromStd(option(result, "--flags"));
        const auto tempo = option(result, "--tempo");
        if (!tempo.empty()) {
            settings.tempo = fromStd(tempo);
        }

        // UTAU reads a UST in the code page it is running under, so the probe is written in the
        // one it will be opened with and not in UTF-8.
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
        cli::Command("utauprobe", "Write a project built to ask UTAU questions")
            .addArgument(cli::Argument("kind", "arguments, or vibrato"))
            .addArgument(cli::Argument("output", "The .ust to write"))
            .addOption(cli::Option({"--voice"},
                                   "The voice bank folder, named the way UTAU names it: the "
                                   "folder alone, under its own voice directory")
                           .arg(cli::Argument("name")))
            .addOption(
                cli::Option({"--out"}, "The wav UTAU should render to").arg(cli::Argument("path")))
            .addOption(cli::Option({"--cache"}, "The folder UTAU should cache into")
                           .arg(cli::Argument("path")))
            .addOption(cli::Option({"--flags"}, "The flags to put on the project itself")
                           .arg(cli::Argument("flags")))
            .addOption(cli::Option({"--tempo"}, "The project's tempo, as UTAU spells it")
                           .arg(cli::Argument("bpm")))
            .addOption(cli::Option({"-c", "--charset"},
                                   "What to write the UST in, which is the code page UTAU will "
                                   "open it under. Defaults to this machine's")
                           .arg(cli::Argument("name")))
            .setHandler(run)
            .addHelpOption(true)
            .addVersionOption("0.0.1"));

    return parser.invoke(system::command_line_arguments());
}
