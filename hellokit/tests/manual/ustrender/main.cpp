/// \file
/// Renders a project to a wav from a terminal, which is what the roadmap's second stage is
/// measured by.
///
/// It needs things a test cannot have: a real voice bank, and the two engines. Those are named
/// on the command line rather than taken from the project, which is the rule the whole synth
/// layer is built around.
///
/// \code
///   ustrender song.ust out.wav --voice "C:/UTAU/voice/uta" --charset Shift_JIS \
///       --resampler C:/UTAU/resampler.exe --wavtool C:/UTAU/wavtool.exe
///   ustrender song.usth out.wav --voice ... --plan
/// \endcode

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include <QtCore/QString>

#include <stdcorelib/support/commandline.h>
#include <stdcorelib/system.h>

#include <hellokit/Document/UstDocument.h>
#include <hellokit/Support/TextCodec.h>
#include <hellokit/Synth/SynthPlan.h>
#include <hellokit/Synth/ThreadedSynthRunner.h>
#include <hellokit/VoiceBank/VoiceBank.h>

using namespace hello::kit;
namespace fs = std::filesystem;

namespace {

    std::string toStd(const QString &text) {
        return text.toStdString();
    }

    QString fromStd(const std::string &text) {
        return QString::fromStdString(text);
    }

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

    bool isUst(const fs::path &path) {
        auto suffix = path.extension().string();
        for (auto &c : suffix) {
            c = char(std::tolower(static_cast<unsigned char>(c)));
        }
        return suffix == ".ust";
    }

    std::optional<Project> readProject(const fs::path &path, const QString &charset,
                                       DiagnosticList &diagnostics) {
        if (!isUst(path)) {
            return Project::open(path, diagnostics);
        }

        const auto ust = UstDocument::open(path, diagnostics);
        if (!ust) {
            return std::nullopt;
        }
        QString settled = charset;
        if (settled.isEmpty()) {
            const auto found = ust->settledCharset();
            if (!found) {
                std::cerr << "error: this UST does not say what encoding it is in. "
                             "Pass --charset with one of:"
                          << std::endl;
                for (const auto &name : TextCodec::ustCandidates()) {
                    std::cerr << "  " << toStd(name) << std::endl;
                }
                return std::nullopt;
            }
            settled = *found;
        }
        return ust->toProject(settled, diagnostics);
    }

    std::string option(const stdc::cli::ParseResult &result, const char *token) {
        return result.valueForOption<std::string>(token).value_or(std::string());
    }

    int render(const stdc::cli::ParseResult &result) {
        const fs::path input = *result.value(0);
        const fs::path output = *result.value(1);
        const QString charset = fromStd(option(result, "--charset"));

        const auto voice = option(result, "--voice");
        if (voice.empty()) {
            std::cerr << "error: --voice says which voice bank to sing with" << std::endl;
            return 1;
        }

        DiagnosticList diagnostics;
        const auto project = readProject(input, charset, diagnostics);
        report(diagnostics);
        if (!project) {
            return 1;
        }

        // The bank's own encoding, which is not the project's. A Shift_JIS UST is routinely sung
        // by a bank in another code page.
        auto bankCharset = fromStd(option(result, "--voice-charset"));
        if (bankCharset.isEmpty()) {
            bankCharset = charset;
        }
        FixedCharsetSelector selector(bankCharset);

        diagnostics.clear();
        const auto bank = VoiceBank::open(fs::path(voice), &selector, diagnostics);
        report(diagnostics);
        if (!bank) {
            return 1;
        }
        std::cout << "voice bank: " << toStd(bank->character().name) << ", "
                  << bank->samples().size() << " samples" << std::endl;

        SynthPlan::Options options;
        options.outputFile = output;
        options.cacheDirectory = output.parent_path() / (output.stem().string() + ".cache");

        diagnostics.clear();
        const auto plan = SynthPlan::make(*project, *bank, options, diagnostics);
        report(diagnostics);
        if (!plan) {
            return 1;
        }
        std::cout << "plan: " << plan->steps().size() << " notes" << std::endl;

        if (result.option("--plan")) {
            // What each engine would be handed, one argument per line, so that a wrong argument
            // is visible without running anything.
            for (const auto &step : plan->steps()) {
                std::cout << "note " << (step.noteIndex + 1) << (step.silent ? " (silent)" : "")
                          << std::endl;
                for (const auto &argument : step.resamplerArguments) {
                    std::cout << "    resampler | " << toStd(argument) << std::endl;
                }
                for (const auto &argument : step.wavtoolArguments) {
                    std::cout << "    wavtool   | " << toStd(argument) << std::endl;
                }
            }
            return 0;
        }

        SynthEngines engines;
        engines.resampler = option(result, "--resampler");
        engines.wavtool = option(result, "--wavtool");
        if (engines.resampler.empty() || engines.wavtool.empty()) {
            std::cerr << "error: --resampler and --wavtool say which engines to run. They are "
                         "never taken from the project."
                      << std::endl;
            return 1;
        }

        ThreadedSynthRunner runner;

        diagnostics.clear();
        const auto outcome = runner.render(*plan, engines, nullptr, diagnostics);
        report(diagnostics);

        std::cout << "resampled " << outcome.resampled << ", silent " << outcome.silent
                  << ", failed " << outcome.failed << std::endl;
        if (!outcome.rendered) {
            return 1;
        }
        std::cout << "wrote " << output.string() << std::endl;
        return 0;
    }

}

int main(int argc, char *argv[]) {
    Q_UNUSED(argc)
    Q_UNUSED(argv)

    using namespace stdc;

    cli::Parser parser(
        cli::Command("ustrender", "Render a project to a wav")
            .addArgument(cli::Argument("input", "The .ust or .usth to render"))
            .addArgument(cli::Argument("output", "The wav to write"))
            .addOption(
                cli::Option({"--voice"}, "The voice bank folder").arg(cli::Argument("folder")))
            .addOption(cli::Option({"-c", "--charset"},
                                   "The encoding of the UST, where the file does not say")
                           .arg(cli::Argument("name")))
            .addOption(cli::Option({"--voice-charset"},
                                   "The encoding of the voice bank, where it differs from the "
                                   "project's")
                           .arg(cli::Argument("name")))
            .addOption(
                cli::Option({"--resampler"}, "The resampler to run").arg(cli::Argument("path")))
            .addOption(cli::Option({"--wavtool"}, "The wavtool to run").arg(cli::Argument("path")))
            .addOption(
                cli::Option({"--plan"}, "Print what each engine would be handed and run nothing"))
            .setHandler(render)
            .addHelpOption(true)
            .addVersionOption("0.0.1"));

    return parser.invoke(system::command_line_arguments());
}
