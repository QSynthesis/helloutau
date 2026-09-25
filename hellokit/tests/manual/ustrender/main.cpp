/// \file
/// Renders a project to a WAV file from the command line. The second roadmap stage is measured
/// by this program.
///
/// It requires resources unavailable to automated tests: a real voice bank and the two engines.
/// These are specified on the command line rather than taken from the project, which is the
/// fundamental rule of the synthesis layer.
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
#include <hellokit/Synth/ClassicSynthRunner.h>
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

    /// A path from the command line.
    ///
    /// The arguments are UTF-8, whereas on Windows a \c fs::path constructed from a narrow
    /// string interprets it in the ANSI code page. Any non-ASCII path would then refer to a
    /// nonexistent file, which affects most voice banks and many projects.
    fs::path pathOf(const std::string &text) {
        return fs::u8path(text);
    }

    /// The extension is taken as UTF-16, because path::string() converts to the code page of the
    /// system on Windows and fails for a character outside it. Only ASCII letters are folded,
    /// because std::tolower depends on the C locale.
    bool isUst(const fs::path &path) {
        auto suffix = path.extension().u16string();
        for (auto &c : suffix) {
            if (c >= u'A' && c <= u'Z') {
                c = char16_t(c - u'A' + u'a');
            }
        }
        return suffix == u".ust";
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
                std::cerr << "error: this UST does not declare its encoding. "
                             "Specify --charset with one of:"
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
        const fs::path input = pathOf(*result.value(0));
        const fs::path output = pathOf(*result.value(1));
        const QString charset = fromStd(option(result, "--charset"));

        const auto voice = option(result, "--voice");
        if (voice.empty()) {
            std::cerr << "error: --voice is required and specifies the voice bank" << std::endl;
            return 1;
        }

        DiagnosticList diagnostics;
        const auto project = readProject(input, charset, diagnostics);
        report(diagnostics);
        if (!project) {
            return 1;
        }

        // The encoding of the voice bank, which is independent of the project encoding. A
        // Shift_JIS UST is commonly rendered with a voice bank in another code page.
        auto bankCharset = fromStd(option(result, "--voice-charset"));
        if (bankCharset.isEmpty()) {
            bankCharset = charset;
        }
        FixedCharsetSelector selector(bankCharset);

        diagnostics.clear();
        const auto bank = VoiceBank::open(pathOf(voice), &selector, diagnostics);
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
            // The arguments of each engine call, one per line, so that an incorrect argument is
            // visible without executing anything.
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
            std::cerr << "error: --resampler and --wavtool are required and specify the engines. "
                         "Engines are never taken from the project."
                      << std::endl;
            return 1;
        }

        // The runner is a compatibility setting, not an implementation detail. See
        // docs/Synth.md.
        std::unique_ptr<SynthRunner> runner;
        if (result.option("--classic")) {
            auto classic = std::make_unique<ClassicSynthRunner>();
            classic->keepScripts = result.option("--keep-scripts").has_value();
            if (result.option("--verbatim")) {
                std::cerr
                    << "warning: --verbatim writes project text into a shell script unescaped, "
                       "which allows the project file to execute commands. It exists "
                       "only for engines that require the exact script text of UTAU."
                    << std::endl;
                classic->quoting = ClassicSynthRunner::Quoting::Verbatim;
            }
            runner = std::move(classic);
        } else {
            runner = std::make_unique<ThreadedSynthRunner>();
        }

        diagnostics.clear();
        const auto outcome = runner->render(*plan, engines, nullptr, diagnostics);
        report(diagnostics);

        std::cout << "resampled " << outcome.resampled << ", reused " << outcome.reused
                  << ", silent " << outcome.silent << ", failed " << outcome.failed << std::endl;
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
        cli::Command("ustrender", "Render a project to a WAV file")
            .addArgument(cli::Argument("input", "The .ust or .usth to render"))
            .addArgument(cli::Argument("output", "The WAV file to write"))
            .addOption(
                cli::Option({"--voice"}, "The voice bank folder").arg(cli::Argument("folder")))
            .addOption(cli::Option({"-c", "--charset"},
                                   "The encoding of the UST, if the file does not declare one")
                           .arg(cli::Argument("name")))
            .addOption(cli::Option({"--voice-charset"},
                                   "The encoding of the voice bank, if it differs from that of the "
                                   "project")
                           .arg(cli::Argument("name")))
            .addOption(
                cli::Option({"--resampler"}, "The resampler to run").arg(cli::Argument("path")))
            .addOption(cli::Option({"--wavtool"}, "The wavtool to run").arg(cli::Argument("path")))
            .addOption(cli::Option(
                {"--plan"}, "Print the arguments of each engine call without executing anything"))
            .addOption(cli::Option({"--classic"},
                                   "Render through temp.bat in a console window, as UTAU does"))
            .addOption(
                cli::Option({"--keep-scripts"}, "Keep temp.bat after rendering, for inspection"))
            .addOption(cli::Option({"--verbatim"},
                                   "Write the script without escaping, as UTAU does. Unsafe"))
            .setHandler(render)
            .addHelpOption(true)
            .addVersionOption("0.0.1"));

    return parser.invoke(system::command_line_arguments());
}
