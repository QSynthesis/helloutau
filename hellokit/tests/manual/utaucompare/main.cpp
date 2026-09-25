/// \file
/// Compares the engine arguments of HelloUtau with those UTAU passed for the same project.
///
/// This is the most demanding check of the project. All other tests verify that the code
/// implements its specification. This one verifies that the specification matches UTAU, which
/// only UTAU itself can confirm.
///
/// **WAV files are deliberately not compared.** The same resampler processing the same sample
/// with a pitch difference of one cent produces entirely different sample values after a few
/// cycles while sounding identical, so a sample-by-sample comparison would report a severe
/// difference where none is audible. The comparison requires agreement of the arguments passed
/// to each engine and of the pitch curve shape in cents.
///
/// Obtaining the UTAU side: render the project in UTAU and, **before closing UTAU**, copy
/// \c temp.bat and \c temp_helper.bat from \c %%TEMP%%\\utauN . UTAU clears that folder on
/// exit. The rendered WAV file is not needed.
///
/// \code
///   utaucompare probe.ust --voice "E:/UTAU/voice/uta" --charset GBK \
///       --script E:/compare/probe-utau1/temp.bat
/// \endcode

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>

#include <QtCore/QString>

#include <stdcorelib/support/commandline.h>
#include <stdcorelib/system.h>

#include <hellokit/Document/UstDocument.h>
#include <hellokit/Support/TextCodec.h>
#include <hellokit/Synth/SynthPlan.h>
#include <hellokit/VoiceBank/VoiceBank.h>

#include "ScriptCompare.h"
#include "UtauScript.h"

using namespace hello::kit;
using namespace utaucompare;
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
            if (diagnostic.severity == DiagnosticSeverity::Note) {
                continue;
            }
            std::cerr << (diagnostic.severity == DiagnosticSeverity::Error ? "error: "
                                                                           : "warning: ")
                      << toStd(diagnostic.message);
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

    /// The value at quantile \a fraction , computed from the histogram.
    int quantile(const std::vector<std::int64_t> &histogram, double fraction) {
        std::int64_t total = 0;
        for (const auto count : histogram) {
            total += count;
        }
        const std::int64_t want = std::int64_t(double(total) * fraction);
        std::int64_t seen = 0;
        for (std::size_t cents = 0; cents < histogram.size(); ++cents) {
            seen += histogram[cents];
            if (seen > want) {
                return int(cents);
            }
        }
        return histogram.empty() ? 0 : int(histogram.size()) - 1;
    }

    void printArguments(const Comparison &comparison, int examples) {
        // Grouped by argument, because one incorrect field is one defect regardless of how many
        // notes it affects, and a list of notes is not a list of defects.
        QStringList order;
        QHash<QString, QList<ArgumentDifference>> byName;
        for (const auto &difference : comparison.arguments) {
            if (!byName.contains(difference.what)) {
                order += difference.what;
            }
            byName[difference.what] += difference;
        }

        std::cout << std::endl
                  << "arguments, out of " << comparison.notesCompared
                  << " notes compared:" << std::endl;
        if (comparison.spelling) {
            std::cout << "  " << comparison.spelling
                      << " differ in text only, because UTAU rounds on output" << std::endl;
        }
        if (order.isEmpty()) {
            std::cout << "  all other arguments are identical" << std::endl;
            return;
        }
        std::sort(order.begin(), order.end(), [&byName](const QString &a, const QString &b) {
            return byName.value(a).size() > byName.value(b).size();
        });
        for (const QString &what : std::as_const(order)) {
            const auto &list = byName.value(what);
            std::cout << "  " << toStd(what.leftJustified(12)) << list.size() << std::endl;
            for (int i = 0; i < std::min(int(list.size()), examples); ++i) {
                std::cout << "      note " << (list.at(i).noteIndex + 1) << "  ours "
                          << toStd(list.at(i).ours) << "  utau " << toStd(list.at(i).theirs)
                          << std::endl;
            }
        }
    }

    void printCurves(const Comparison &comparison, int examples) {
        std::int64_t readings = 0;
        for (const auto count : comparison.readings) {
            readings += count;
        }
        std::cout << std::endl
                  << "pitch curves: " << comparison.curves.size() << " notes, " << readings
                  << " readings" << std::endl;
        if (readings == 0) {
            return;
        }

        double total = 0;
        for (std::size_t cents = 0; cents < comparison.readings.size(); ++cents) {
            total += double(cents) * double(comparison.readings[cents]);
        }
        std::cout << "  readings: median " << quantile(comparison.readings, 0.5) << "  mean "
                  << (total / double(readings)) << "  98% within "
                  << quantile(comparison.readings, 0.98) << "  worst "
                  << (comparison.readings.empty() ? 0 : int(comparison.readings.size()) - 1)
                  << " cents" << std::endl;

        const int thresholds[] = {0, 2, 5, 10, 50};
        const char *labels[] = {"the same", "within 2", "within 5", "within 10", "within 50"};
        int counted[std::size(thresholds) + 1] = {};
        for (const auto &curve : comparison.curves) {
            std::size_t which = std::size(thresholds);
            for (std::size_t i = 0; i < std::size(thresholds); ++i) {
                if (curve.deviation.peak <= thresholds[i]) {
                    which = i;
                    break;
                }
            }
            counted[which]++;
        }
        std::cout << "  notes:";
        for (std::size_t i = 0; i < std::size(thresholds); ++i) {
            if (counted[i]) {
                std::cout << "  " << labels[i] << " " << counted[i];
            }
        }
        if (counted[std::size(thresholds)]) {
            std::cout << "  further off " << counted[std::size(thresholds)];
        }
        std::cout << std::endl;

        auto worst = comparison.curves;
        std::sort(worst.begin(), worst.end(),
                  [](const CurveDifference &a, const CurveDifference &b) {
                      return a.deviation.peak > b.deviation.peak;
                  });
        for (int i = 0; i < std::min(int(worst.size()), examples); ++i) {
            const auto &curve = worst.at(i);
            if (curve.deviation.peak == 0) {
                break;
            }
            std::cout << "      note " << (curve.noteIndex + 1) << "  peak " << curve.deviation.peak
                      << " cents at reading " << curve.deviation.peakAt << "  mean "
                      << curve.deviation.mean << "  readings " << curve.ourReadings << " against "
                      << curve.theirReadings << std::endl;
        }
    }

    int run(const stdc::cli::ParseResult &result) {
        const fs::path input = pathOf(*result.value(0));
        const QString charset = fromStd(option(result, "--charset"));

        const auto voice = option(result, "--voice");
        const auto script = option(result, "--script");
        if (voice.empty() || script.empty()) {
            std::cerr << "error: --voice and --script are required and specify the voice bank and "
                         "the temp.bat written by UTAU"
                      << std::endl;
            return 1;
        }

        DiagnosticList diagnostics;
        const auto project = readProject(input, charset, diagnostics);
        report(diagnostics);
        if (!project) {
            return 1;
        }

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
        const auto output = option(result, "--output");
        options.outputFile = output.empty() ? input.parent_path() / "temp.wav" : pathOf(output);
        const auto cache = option(result, "--cache");
        options.cacheDirectory = cache.empty()
                                     ? input.parent_path() / (input.stem().string() + ".cache")
                                     : pathOf(cache);

        diagnostics.clear();
        const auto plan = SynthPlan::make(*project, *bank, options, diagnostics);
        report(diagnostics);
        if (!plan) {
            return 1;
        }
        std::cout << "plan: " << plan->steps().size() << " notes" << std::endl;

        QString error;
        const auto calls =
            readScript(pathOf(script), fromStd(option(result, "--script-charset")), &error);
        if (!calls) {
            std::cerr << "error: " << toStd(error) << std::endl;
            return 1;
        }
        int withResampler = 0;
        for (const auto &call : *calls) {
            withResampler += call.resamplerArguments.isEmpty() ? 0 : 1;
        }
        std::cout << "script: " << calls->size() << " calls, " << withResampler
                  << " with a resampler" << std::endl;

        const auto comparison = compare(plan->steps(), *calls);
        if (comparison.onlyOurs || comparison.onlyTheirs) {
            std::cout << "  " << comparison.onlyOurs << " notes without a call in the UTAU script, "
                      << comparison.onlyTheirs << " calls without a note in HelloUtau" << std::endl;
        }

        const int examples = result.valueForOption<int>("--examples").value_or(4);
        printArguments(comparison, examples);
        printCurves(comparison, examples);
        return 0;
    }

}

int main(int argc, char *argv[]) {
    Q_UNUSED(argc)
    Q_UNUSED(argv)

    using namespace stdc;

    cli::Parser parser(
        cli::Command("utaucompare", "Compare the engine calls of HelloUtau with those of UTAU")
            .addArgument(cli::Argument("input", "The .ust or .usth rendered by both sides"))
            .addOption(
                cli::Option({"--voice"}, "The voice bank folder").arg(cli::Argument("folder")))
            .addOption(cli::Option({"--script"},
                                   "The temp.bat written by UTAU, with temp_helper.bat "
                                   "in the same directory")
                           .arg(cli::Argument("path")))
            .addOption(cli::Option({"-c", "--charset"},
                                   "The encoding of the UST, if the file does not declare one")
                           .arg(cli::Argument("name")))
            .addOption(cli::Option({"--voice-charset"},
                                   "The encoding of the voice bank, if it differs from that of the "
                                   "project")
                           .arg(cli::Argument("name")))
            .addOption(cli::Option({"--script-charset"},
                                   "The encoding of the script, which is the ANSI code page of the "
                                   "machine UTAU ran on. Defaults to that of this machine")
                           .arg(cli::Argument("name")))
            .addOption(
                cli::Option({"--cache"},
                            "The cache directory of UTAU, so that the cache file names match")
                    .arg(cli::Argument("folder")))
            .addOption(cli::Option({"--output"}, "The WAV file UTAU rendered to")
                           .arg(cli::Argument("path")))
            .addOption(
                cli::Option({"--examples"}, "The number of example notes listed per difference")
                    .arg(cli::Argument("count")))
            .setHandler(run)
            .addHelpOption(true)
            .addVersionOption("0.0.1"));

    return parser.invoke(system::command_line_arguments());
}
