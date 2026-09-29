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
///   ustrender song.ust out.wav --voice ... --compare-pitch
///   ustrender song.ust out.wav --voice ... --resampler ... --wavtool ... --compare-mix
/// \endcode

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include <QtCore/QString>

#include <stdcorelib/console.h>
#include <stdcorelib/path.h>
#include <stdcorelib/support/commandline.h>
#include <stdcorelib/system.h>

#include <hellokit/Document/TempoMap.h>
#include <hellokit/Document/UstDocument.h>
#include <hellokit/Support/TextCodec.h>
#include <hellokit/Synth/PitchCurve.h>
#include <hellokit/Synth/SynthPlan.h>
#include <hellokit/Synth/ClassicSynthRunner.h>
#include <hellokit/Synth/RealtimeSynth.h>
#include <hellokit/Synth/ThreadedSynthRunner.h>
#include <hellokit/Synth/WaveAudio.h>
#include <hellokit/Synth/WavtoolMixer.h>
#include <hellokit/VoiceBank/VoiceBank.h>

#include "ScriptExport.h"

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
            const auto note = diagnostic.noteIndex
                                  ? " (note " + std::to_string(*diagnostic.noteIndex + 1) + ")"
                                  : std::string();
            stdc::console::u8fprintf(stderr, "%s: %s%s\n", level, toStd(diagnostic.message).c_str(),
                                     note.c_str());
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
                stdc::console::u8fputs("error: this UST does not declare its encoding. "
                                       "Specify --charset with one of:\n",
                                       stderr);
                for (const auto &name : TextCodec::ustCandidates()) {
                    stdc::console::u8fprintf(stderr, "  %s\n", toStd(name).c_str());
                }
                return std::nullopt;
            }
            settled = *found;
        }
        return ust->toProject(settled, diagnostics);
    }

    /// Reads a 16-bit WAV file as its samples, or an empty list.
    std::vector<qint16> samplesOf(const fs::path &path) {
        DiagnosticList ignored;
        const auto audio = WaveAudio::read(path, ignored);
        std::vector<qint16> samples;
        if (audio) {
            samples.reserve(audio->samples.size());
            for (const float sample : audio->samples) {
                samples.push_back(qint16(std::lround(sample * 32768)));
            }
        }
        return samples;
    }

    /// Concatenates the fragments of \a plan in the process and compares the result with the
    /// track file the wavtool wrote. See the section on realtime rendering in docs/Synth.md for
    /// the permitted difference.
    int compareMix(const SynthPlan &plan, const fs::path &output) {
        QList<WavtoolCall> calls;
        std::vector<std::vector<qint16>> fragments;
        for (const auto &step : plan.steps()) {
            const auto call = WavtoolCall::parse(step.wavtoolArguments);
            if (!call) {
                stdc::console::u8fprintf(stderr,
                                         "error: note %d has wavtool arguments that "
                                         "cannot be read\n",
                                         step.noteIndex + 1);
                return 1;
            }
            calls.push_back(*call);
            fragments.push_back(step.silent ? std::vector<qint16>() : samplesOf(step.cacheFile));
        }
        const auto segments = WavtoolMixer::layOut(calls);
        std::vector<qint16> mixed(size_t(WavtoolMixer::lengthOf(segments)));
        WavtoolMixer::mix(
            segments,
            [&](int index) {
                const auto &fragment = fragments[size_t(index)];
                return fragment.empty() ? nullptr : &fragment;
            },
            0, qint64(mixed.size()), mixed.data());

        // The result beside the track file, as raw 16-bit samples, for a closer look
        auto raw = output;
        raw += ".mixed.raw";
        std::ofstream(raw, std::ios::binary)
            .write(reinterpret_cast<const char *>(mixed.data()),
                   std::streamsize(mixed.size() * sizeof(qint16)));

        const auto written = samplesOf(output);
        const size_t common = std::min(written.size(), mixed.size());
        int largest = 0;
        size_t differing = 0;
        size_t firstDifference = common;
        for (size_t i = 0; i < common; ++i) {
            const int difference = std::abs(int(written[i]) - int(mixed[i]));
            if (difference > 0) {
                ++differing;
                firstDifference = std::min(firstDifference, i);
            }
            largest = std::max(largest, difference);
        }
        stdc::u8printf("compare: wavtool %zu samples, in process %zu samples\n", written.size(),
                       mixed.size());
        stdc::u8printf("compare: %zu samples differ, the largest by %d", differing, largest);
        if (differing > 0) {
            stdc::u8printf(", the first at %zu (%.3f ms)", firstDifference,
                           double(firstDifference) * 1000 / WavtoolMixer::sampleRate);
        }
        stdc::u8printf("\n");
        return written.size() == mixed.size() && largest <= 1 ? 0 : 2;
    }

    /// Computes the pitch curve of each note of \a plan with PitchCurve, as the editor draws it,
    /// and compares it with the curve the resampler receives, which must agree value for value.
    int comparePitch(const Project &project, const SynthPlan &plan) {
        const auto &notes = project.tracks.first().notes;
        const auto tempos = TempoMap::of(project);
        const auto &steps = plan.steps();
        int differing = 0;
        qsizetype values = 0;
        for (qsizetype i = 0; i < steps.size(); ++i) {
            const auto &step = steps[i];
            PitchCurve::Timing timing;
            timing.preUtterance = step.preUtterance;
            timing.startPoint = step.startPoint;
            if (i + 1 < steps.size()) {
                timing.nextPreUtterance = steps[i + 1].preUtterance;
                timing.nextOverlap = steps[i + 1].voiceOverlap;
            }
            const auto computed =
                PitchCurve(notes, step.noteIndex, tempos.tempo(step.noteIndex)).values(timing);
            values += computed.size();
            if (computed != step.pitch) {
                ++differing;
                stdc::u8printf("pitch: note %d differs\n", step.noteIndex + 1);
            }
        }
        stdc::u8printf("pitch: %d notes, %d values, %d notes differ\n", int(steps.size()),
                       int(values), differing);
        return differing == 0 ? 0 : 2;
    }

    /// Renders \a plan as realtime playback does, from its start, and writes the whole track as a
    /// 16-bit WAV file, which only the wavtool of UTAU writes otherwise.
    int renderRealtime(const SynthPlan &plan, const SynthEngines &engines, const fs::path &output) {
        const auto started = std::chrono::steady_clock::now();
        RealtimeSynth synth(engines);
        synth.setPlan(plan);
        const qint64 length = synth.length();

        // The first second is what playback waits for before it starts.
        synth.waitReady(0, std::min<qint64>(length, WavtoolMixer::sampleRate),
                        std::chrono::minutes(5));
        const auto firstSecond = std::chrono::steady_clock::now() - started;
        if (!synth.waitReady(0, length, std::chrono::minutes(30))) {
            stdc::console::u8fputs("error: the notes were not rendered in time\n", stderr);
            return 1;
        }
        const auto whole = std::chrono::steady_clock::now() - started;
        report(synth.takeDiagnostics());

        std::vector<qint16> samples(static_cast<size_t>(length));
        synth.mix(0, length, samples.data());
        const auto bytes = quint32(samples.size() * sizeof(qint16));
        std::ofstream out(output, std::ios::binary);
        const auto u32 = [&out](quint32 value) { out.write(reinterpret_cast<char *>(&value), 4); };
        const auto u16 = [&out](quint16 value) { out.write(reinterpret_cast<char *>(&value), 2); };
        out.write("RIFF", 4);
        u32(36 + bytes);
        out.write("WAVEfmt ", 8);
        u32(16);
        u16(1);
        u16(1);
        u32(WavtoolMixer::sampleRate);
        u32(WavtoolMixer::sampleRate * 2);
        u16(2);
        u16(16);
        out.write("data", 4);
        u32(bytes);
        out.write(reinterpret_cast<const char *>(samples.data()), std::streamsize(bytes));

        using std::chrono::duration_cast;
        using std::chrono::milliseconds;
        stdc::u8printf("realtime: the first second after %lld ms, all %d notes after %lld ms\n",
                       qint64(duration_cast<milliseconds>(firstSecond).count()),
                       int(plan.steps().size()),
                       qint64(duration_cast<milliseconds>(whole).count()));
        stdc::u8printf("wrote %s\n", stdc::path::to_utf8(output).c_str());
        return 0;
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
            stdc::console::u8fputs("error: --voice is required and specifies the voice bank\n",
                                   stderr);
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
        stdc::u8printf("voice bank: %s, %d samples\n", toStd(bank->character().name).c_str(),
                       int(bank->samples().size()));

        SynthPlan::Options options;
        options.outputFile = output;
        options.cacheDirectory = output.parent_path() / output.stem();
        options.cacheDirectory += ".cache";

        diagnostics.clear();
        const auto plan = SynthPlan::make(*project, *bank, options, diagnostics);
        report(diagnostics);
        if (!plan) {
            return 1;
        }
        stdc::u8printf("plan: %d notes\n", int(plan->steps().size()));

        if (result.option("--plan")) {
            // The arguments of each engine call, one per line, so that an incorrect argument is
            // visible without executing anything.
            for (const auto &step : plan->steps()) {
                stdc::u8printf("note %d%s\n", step.noteIndex + 1, step.silent ? " (silent)" : "");
                for (const auto &argument : step.resamplerArguments) {
                    stdc::u8printf("    resampler | %s\n", toStd(argument).c_str());
                }
                for (const auto &argument : step.wavtoolArguments) {
                    stdc::u8printf("    wavtool   | %s\n", toStd(argument).c_str());
                }
            }
            return 0;
        }

        if (result.option("--compare-pitch")) {
            return comparePitch(*project, *plan);
        }

        SynthEngines engines;
        engines.resampler = pathOf(option(result, "--resampler"));
        engines.wavtool = pathOf(option(result, "--wavtool"));
        if (engines.resampler.empty() || engines.wavtool.empty()) {
            stdc::console::u8fputs("error: --resampler and --wavtool are required and specify the "
                                   "engines. Engines are never taken from the project.\n",
                                   stderr);
            return 1;
        }

        if (result.option("--realtime")) {
            return renderRealtime(*plan, engines, output);
        }

        // The runner is a compatibility setting, not an implementation detail. See
        // docs/Synth.md.
        std::unique_ptr<SynthRunner> runner;
        if (result.option("--classic")) {
            auto classic = std::make_unique<ClassicSynthRunner>();
            classic->keepScripts = result.option("--keep-scripts").has_value();
            if (result.option("--verbatim")) {
                stdc::console::u8fputs(
                    "warning: --verbatim writes project text into a shell script unescaped, "
                    "which allows the project file to execute commands. It exists only for "
                    "engines that require the exact script text of UTAU.\n",
                    stderr);
                classic->quoting = ClassicSynthRunner::Quoting::Verbatim;
            }
            runner = std::move(classic);
        } else {
            runner = std::make_unique<ThreadedSynthRunner>();
        }

        diagnostics.clear();
        const auto outcome = runner->render(*plan, engines, nullptr, diagnostics);
        report(diagnostics);

        stdc::u8printf("resampled %d, reused %d, silent %d, failed %d\n", outcome.resampled,
                       outcome.reused, outcome.silent, outcome.failed);
        if (!outcome.rendered) {
            return 1;
        }
        stdc::u8printf("wrote %s\n", stdc::path::to_utf8(output).c_str());

        if (result.option("--compare-mix")) {
            return compareMix(*plan, output);
        }
        return 0;
    }

    QStringList listOption(const stdc::cli::ParseResult &result, const char *token) {
        QStringList out;
        if (const auto given = result.option(token)) {
            for (const auto &value :
                 given->values<std::string>(0).value_or(std::vector<std::string>())) {
                out.push_back(fromStd(value));
            }
        }
        return out;
    }

    // The scripts that run the engines of a render one call after another, for comparing
    // environments. See docs/claude/render-comparison.md.
    int script(const stdc::cli::ParseResult &result) {
        const fs::path input = pathOf(*result.value(0));
        const auto output = fromStd(*result.value(1));
        const QString charset = fromStd(option(result, "--charset"));

        ScriptExport exporter;
        const auto target = option(result, "--target");
        if (target == "windows") {
            exporter.target = ScriptExport::Windows;
        } else if (target == "linux") {
            exporter.target = ScriptExport::Linux;
        } else {
            stdc::console::u8fputs("error: --target is windows or linux\n", stderr);
            return 1;
        }
        exporter.project = fromStd(*result.value(0));
        exporter.voice = pathOf(option(result, "--voice"));
        exporter.voiceAs = fromStd(option(result, "--voice-as"));
        exporter.scriptDirectory = fromStd(option(result, "--script-dir"));
        exporter.emitDirectory = pathOf(option(result, "--emit-dir"));
        exporter.snapshotDirectory = fromStd(option(result, "--snapshots"));
        exporter.resamplerCommand = listOption(result, "--resampler-command");
        exporter.wavtoolCommand = listOption(result, "--wavtool-command");
        exporter.lastNote = result.option("--last-note").has_value();
        if (exporter.voice.empty() || exporter.scriptDirectory.isEmpty()) {
            stdc::console::u8fputs("error: --voice and --script-dir are required\n", stderr);
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
        const auto bank = VoiceBank::open(exporter.voice, &selector, diagnostics);
        report(diagnostics);
        if (!bank) {
            return 1;
        }

        // The paths as the script writes them. They need not exist here: for Linux they are
        // paths of the other system.
        SynthPlan::Options options;
        options.outputFile = fs::path(output.toStdU16String());
        const auto cache = option(result, "--cache");
        options.cacheDirectory =
            cache.empty() ? fs::path((output + QStringLiteral(".cache")).toStdU16String())
                          : fs::path(fromStd(cache).toStdU16String());
        diagnostics.clear();
        const auto plan = SynthPlan::make(*project, *bank, options, diagnostics);
        report(diagnostics);
        if (!plan) {
            return 1;
        }
        stdc::u8printf("plan: %d notes\n", int(plan->steps().size()));
        return exporter.write(*plan);
    }

    int compare(const stdc::cli::ParseResult &result) {
        return compareManifests(pathOf(*result.value(0)), pathOf(*result.value(1)));
    }

    int copy(const stdc::cli::ParseResult &result) {
        return copyVoice(pathOf(*result.value(0)), pathOf(*result.value(1)));
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
            .addOption(cli::Option({"--realtime"},
                                   "Render as realtime playback does, concatenating in the "
                                   "process instead of running the wavtool"))
            .addOption(cli::Option({"--compare-pitch"},
                                   "Compare the pitch curve the editor draws with that of the "
                                   "resampler, without executing anything"))
            .addOption(cli::Option({"--compare-mix"},
                                   "Concatenate the fragments in the process as well, and compare "
                                   "the result with the file the wavtool wrote"))
            .setHandler(render)
            .addCommand(
                cli::Command("script",
                             "Write the scripts of a render, a call at a time, with a manifest, "
                             "for comparing the engines in two environments")
                    .addArgument(cli::Argument("input", "The .ust or .usth to render"))
                    .addArgument(
                        cli::Argument("output", "The track file, as the script refers to it"))
                    .addOption(cli::Option({"--target"}, "windows for temp.bat, linux for temp.sh")
                                   .arg(cli::Argument("system")))
                    .addOption(cli::Option({"--voice"}, "The voice bank folder to read")
                                   .arg(cli::Argument("folder")))
                    .addOption(cli::Option({"--voice-as"},
                                           "The voice bank folder as the script refers to it, if "
                                           "another")
                                   .arg(cli::Argument("folder")))
                    .addOption(cli::Option({"-c", "--charset"},
                                           "The encoding of the UST, if the file does not declare "
                                           "one")
                                   .arg(cli::Argument("name")))
                    .addOption(
                        cli::Option({"--voice-charset"},
                                    "The encoding of the voice bank, if it differs from that "
                                    "of the project")
                            .arg(cli::Argument("name")))
                    .addOption(cli::Option({"--cache"}, "The folder of the fragments")
                                   .arg(cli::Argument("folder")))
                    .addOption(cli::Option({"--script-dir"},
                                           "The folder of the scripts and the working folder of "
                                           "the engines, as the script refers to it")
                                   .arg(cli::Argument("folder")))
                    .addOption(cli::Option({"--emit-dir"},
                                           "Where to write the files, if not the script folder")
                                   .arg(cli::Argument("folder")))
                    .addOption(cli::Option({"--snapshots"},
                                           "The folder for a copy of what each call wrote")
                                   .arg(cli::Argument("folder")))
                    .addOption(cli::Option({"--resampler-command"},
                                           "The program and the arguments before those of a call")
                                   .arg(cli::Argument("argument").multi()))
                    .addOption(cli::Option({"--wavtool-command"},
                                           "The program and the arguments before those of a call")
                                   .arg(cli::Argument("argument").multi()))
                    .addOption(cli::Option({"--last-note"},
                                           "Pass LAST_NOTE to the wavtool with the last sung note"))
                    .setHandler(script))
            .addCommand(cli::Command("compare-manifests",
                                     "Compare two manifests without the roots of their paths")
                            .addArgument(cli::Argument("first", "A manifest.json"))
                            .addArgument(cli::Argument("second", "Another manifest.json"))
                            .setHandler(compare))
            .addCommand(cli::Command("copy-voice",
                                     "Copy a voice bank without the files engines derive from its "
                                     "samples")
                            .addArgument(cli::Argument("from", "The voice bank folder"))
                            .addArgument(cli::Argument("to", "A new folder"))
                            .setHandler(copy))
            .addHelpOption(true)
            .addVersionOption("0.0.1"));

    return parser.invoke(system::command_line_arguments());
}
