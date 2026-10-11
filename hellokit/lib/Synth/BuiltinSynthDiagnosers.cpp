#include "BuiltinSynthDiagnosers.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>

#include <hellokit/Support/TextCodec.h>

namespace fs = std::filesystem;

namespace hello::kit {

    namespace {

        // The texts of all diagnosers of this file share one context.
        class Text {
            Q_DECLARE_TR_FUNCTIONS(hello::kit::BuiltinSynthDiagnosers)
        };

        QString displayed(const fs::path &path) {
            return QDir::toNativeSeparators(QString::fromStdU16String(path.u16string()));
        }

        // The texts that a call passes as paths: the program and every argument. The other
        // arguments, such as the pitch and the flags, are ASCII and pass every check.
        QStringList pathsOf(const SynthFailure &failure) {
            return QStringList{displayed(failure.program)} + failure.arguments;
        }

        bool lacksOutput(const SynthFailure &failure) {
            return failure.stage == SynthFailure::Resampler ||
                   failure.stage == SynthFailure::Wavtool;
        }

        // Collects the failures for which key returns a value into one group per value, in the
        // order of their first failure.
        template <class Key, class F>
        std::vector<std::pair<Key, QList<int>>> groups(const QList<SynthFailure> &failures, F key) {
            std::vector<std::pair<Key, QList<int>>> result;
            for (int i = 0; i < failures.size(); ++i) {
                const std::optional<Key> value = key(failures.at(i));
                if (!value) {
                    continue;
                }
                auto group = std::find_if(result.begin(), result.end(),
                                          [&](const auto &item) { return item.first == *value; });
                if (group == result.end()) {
                    result.push_back({*value, {}});
                    group = result.end() - 1;
                }
                group->second.push_back(i);
            }
            return result;
        }

        class CodePage : public SynthDiagnoser {
        public:
            QString id() const override {
                return QStringLiteral("codePage");
            }

            int priority() const override {
                return -10;
            }

            QList<SynthDiagnosis> diagnose(const SynthDiagnosisContext &context) const override {
#ifdef Q_OS_WINDOWS
                const TextCodec codec(TextCodec::systemName());
                QString example;
                QList<int> failures;
                for (int i = 0; i < context.failures.size(); ++i) {
                    const auto &failure = context.failures.at(i);
                    if (!lacksOutput(failure)) {
                        continue;
                    }
                    for (const auto &text : pathsOf(failure)) {
                        if (!codec.canEncode(text)) {
                            if (example.isEmpty()) {
                                example = text;
                            }
                            failures.push_back(i);
                            break;
                        }
                    }
                }
                if (failures.isEmpty()) {
                    return {};
                }
                return {
                    { Text::tr("A path contains characters that the code page of the system "
                               "cannot represent."),
                     Text::tr("A synth tool that reads its arguments in the code page of the "
                               "system receives these characters as substitute characters, and "
                               "therefore cannot open the file. The path is %1.")
                          .arg(example),
                     {Text::tr("Move the voice bank, the project or the cache folder to a path "
                                "without these characters."),
                       Text::tr("Change the language for non-Unicode programs of Windows to the "
                                "language of the path.")},
                     failures, }
                };
#else
                Q_UNUSED(context);
                return {};
#endif
            }
        };

        class Start : public SynthDiagnoser {
        public:
            QString id() const override {
                return QStringLiteral("start");
            }

            int priority() const override {
                return -20;
            }

            QList<SynthDiagnosis> diagnose(const SynthDiagnosisContext &context) const override {
                QList<SynthDiagnosis> result;
                for (const auto &[program, failures] :
                     groups<fs::path>(context.failures, [](const SynthFailure &failure) {
                         return failure.run && !failure.run->started
                                    ? std::optional<fs::path>(failure.program)
                                    : std::nullopt;
                     })) {
                    result.push_back({
                        Text::tr("The synth tool %1 could not be started.").arg(displayed(program)),
                        Text::tr("The file does not exist, is not a program, or is blocked by "
                                 "the system or by an antivirus program."),
                        {Text::tr("Check that the file exists, and select the synth tool again "
                                  "in the rendering settings.")},
                        failures,
                    });
                }
                return result;
            }
        };

        class ExitStatus : public SynthDiagnoser {
        public:
            QString id() const override {
                return QStringLiteral("exitStatus");
            }

            int priority() const override {
                return -30;
            }

            QList<SynthDiagnosis> diagnose(const SynthDiagnosisContext &context) const override {
#ifdef Q_OS_WINDOWS
                // The NTSTATUS values with which Windows ends a process that fails to load or
                // crashes, from ntstatus.h
                constexpr quint32 dllNotFound = 0xC0000135;
                constexpr quint32 invalidImageFormat = 0xC000007B;
                constexpr quint32 accessViolation = 0xC0000005;
                QList<SynthDiagnosis> result;
                using Key = std::pair<quint32, fs::path>;
                for (const auto &[key, failures] :
                     groups<Key>(context.failures, [&](const SynthFailure &failure) {
                         if (!failure.run || !failure.run->started || failure.run->timedOut) {
                             return std::optional<Key>();
                         }
                         const auto code = quint32(failure.run->exitCode);
                         if (code != dllNotFound && code != invalidImageFormat &&
                             code != accessViolation) {
                             return std::optional<Key>();
                         }
                         return std::optional<Key>(Key{code, failure.program});
                     })) {
                    const auto program = displayed(key.second);
                    if (key.first == dllNotFound) {
                        result.push_back({
                            Text::tr("The synth tool %1 requires a DLL that cannot be found.")
                                .arg(program),
                            Text::tr("Windows ended the synth tool with the status 0xC0000135, "
                                     "STATUS_DLL_NOT_FOUND."),
                            {Text::tr("Place the DLLs supplied with the synth tool in its "
                                      "folder."),
                               Text::tr("Install the runtime that the synth tool requires, such as "
                                      "the Microsoft Visual C++ Redistributable.")},
                            failures,
                        });
                    } else if (key.first == invalidImageFormat) {
                        result.push_back({
                            Text::tr("The synth tool %1 or a DLL that it loads has the wrong "
                                     "image format.")
                                .arg(program),
                            Text::tr("Windows ended the synth tool with the status 0xC000007B, "
                                     "STATUS_INVALID_IMAGE_FORMAT. A 32-bit DLL in a 64-bit "
                                     "program, or the reverse, causes it."),
                            {Text::tr("Replace the DLLs in the folder of the synth tool with "
                                      "those of the same release.")},
                            failures,
                        });
                    } else {
                        result.push_back({
                            Text::tr("The synth tool %1 crashed.").arg(program),
                            Text::tr("Windows ended the synth tool with the status 0xC0000005, "
                                     "STATUS_ACCESS_VIOLATION. The synth tool cannot process "
                                     "this input, or it is defective."),
                            {Text::tr("Check the samples of these notes in the voice bank "
                                      "editor."),
                               Text::tr("Use another release of the synth tool.")},
                            failures,
                        });
                    }
                }
                return result;
#else
                Q_UNUSED(context);
                return {};
#endif
            }
        };

        class Timeout : public SynthDiagnoser {
        public:
            QString id() const override {
                return QStringLiteral("timeout");
            }

            int priority() const override {
                return -30;
            }

            QList<SynthDiagnosis> diagnose(const SynthDiagnosisContext &context) const override {
                QList<SynthDiagnosis> result;
                for (const auto &[program, failures] :
                     groups<fs::path>(context.failures, [](const SynthFailure &failure) {
                         return failure.run && failure.run->timedOut
                                    ? std::optional<fs::path>(failure.program)
                                    : std::nullopt;
                     })) {
                    result.push_back({
                        Text::tr("The synth tool %1 did not finish within the time limit.")
                            .arg(displayed(program)),
                        Text::tr("The synth tool was stopped. It may be waiting for input or "
                                 "showing a window, or require a long time for a long note."),
                        {Text::tr("Run the synth tool once from a console, and check whether it "
                                  "waits for input or shows a message.")},
                        failures,
                    });
                }
                return result;
            }
        };

        class LongPath : public SynthDiagnoser {
        public:
            QString id() const override {
                return QStringLiteral("longPath");
            }

            int priority() const override {
                return -40;
            }

            QList<SynthDiagnosis> diagnose(const SynthDiagnosisContext &context) const override {
#ifdef Q_OS_WINDOWS
                // MAX_PATH, including the terminating null character
                constexpr qsizetype maximumLength = 259;
                QString example;
                QList<int> failures;
                for (int i = 0; i < context.failures.size(); ++i) {
                    const auto &failure = context.failures.at(i);
                    if (!lacksOutput(failure)) {
                        continue;
                    }
                    for (const auto &text : pathsOf(failure)) {
                        if (text.size() > maximumLength) {
                            if (example.isEmpty()) {
                                example = text;
                            }
                            failures.push_back(i);
                            break;
                        }
                    }
                }
                if (failures.isEmpty()) {
                    return {};
                }
                return {
                    { Text::tr("A path is longer than 260 characters."),
                     Text::tr("A synth tool without support for long paths cannot open it. The "
                               "path is %1.")
                          .arg(example),
                     {Text::tr("Move the voice bank, the project or the cache folder to a "
                                "shorter path.")},
                     failures, }
                };
#else
                Q_UNUSED(context);
                return {};
#endif
            }
        };

        // The printable ASCII characters at the start of head, for the identification of the
        // file by the user
        QString printableStart(const QByteArray &head) {
            QString text;
            for (const auto c : head.first(std::min<qsizetype>(head.size(), 48))) {
                text += (c >= 0x20 && c < 0x7F) ? QChar(c) : QChar(u'.');
            }
            return text;
        }

        class Output : public SynthDiagnoser {
        public:
            QString id() const override {
                return QStringLiteral("output");
            }

            int priority() const override {
                return -1000;
            }

            QList<SynthDiagnosis> diagnose(const SynthDiagnosisContext &context) const override {
                enum Kind { NotWave, UnreadableWave, WavtoolNote, NoTrack, ExitCode, NoFragment };
                using Key = std::pair<int, fs::path>;
                QList<SynthDiagnosis> result;
                for (const auto &[key, failures] :
                     groups<Key>(context.failures, [](const SynthFailure &failure) {
                         // Reported as the failure of the track, which is recorded separately
                         if (failure.stage == SynthFailure::Unconfirmed) {
                             return std::optional<Key>();
                         }
                         const auto kind = [&] {
                             switch (failure.stage) {
                                 case SynthFailure::Fragment:
                                     return SynthFailure::isWave(failure.fileHead) ? UnreadableWave
                                                                                   : NotWave;
                                 case SynthFailure::Wavtool:
                                     return failure.noteIndex ? WavtoolNote : NoTrack;
                                 case SynthFailure::Resampler:
                                 case SynthFailure::Unconfirmed:
                                     break;
                             }
                             const bool exited = failure.run && failure.run->started &&
                                                 !failure.run->timedOut && !failure.run->cancelled;
                             return exited && failure.run->exitCode != 0 ? ExitCode : NoFragment;
                         }();
                         return std::optional<Key>(Key{kind, failure.program});
                     })) {
                    const auto &first = context.failures.at(failures.first());
                    const auto program = displayed(key.second);
                    const auto output = first.run ? first.run->output.trimmed() : QString();
                    // The remedies of a resampler that wrote no fragment
                    const QStringList resamplerRemedies{
                        Text::tr("Check the samples of these notes in the voice bank editor."),
                        Text::tr("Use another resampler."),
                    };
                    switch (key.first) {
                        case NotWave:
                            result.push_back({
                                Text::tr("Realtime playback cannot read the fragments of the "
                                         "resampler %1, which are not wave files.")
                                    .arg(program),
                                Text::tr("Realtime playback joins the fragments itself and reads "
                                         "only wave files, so that these notes are silent. The "
                                         "file %1 begins with \"%2\".")
                                    .arg(displayed(first.file), printableStart(first.fileHead)),
                                {Text::tr("Check the settings of the resampler for an option "
                                          "that changes the format of its output."),
                                                                                            Text::tr("Play in a prerender mode, in which the wavtool joins "
                                          "the fragments.")},
                                failures,
                            });
                            break;
                        case UnreadableWave:
                            result.push_back({
                                Text::tr("The resampler %1 wrote wave files that cannot be read.")
                                    .arg(program),
                                Text::tr("These notes are silent. The file %1 is a wave file "
                                         "whose format or content cannot be read.")
                                    .arg(displayed(first.file)),
                                {Text::tr("Check the settings of the resampler for its output "
                                          "format, such as the bit depth."),
                                                            Text::tr("Render the project again after clearing the cache, in "
                                          "case the file is incomplete.")},
                                failures,
                            });
                            break;
                        case WavtoolNote: {
                            auto explanation =
                                !first.run ? Text::tr("The wavtool appended nothing to the track.")
                                : output.isEmpty()
                                    ? Text::tr("The wavtool reported the exit code %1 and "
                                               "appended nothing to the track.")
                                          .arg(first.run->exitCode)
                                    : Text::tr("The wavtool reported the exit code %1 and "
                                               "appended nothing to the track. Its output is: %2")
                                          .arg(first.run->exitCode)
                                          .arg(output);
                            if (first.fileSize >= 0 && !SynthFailure::isWave(first.fileHead)) {
                                explanation +=
                                    QLatin1Char(' ') +
                                    Text::tr("The fragment %1 is not a wave file. It "
                                             "begins with \"%2\".")
                                        .arg(displayed(first.file), printableStart(first.fileHead));
                            }
                            result.push_back({
                                Text::tr("The wavtool %1 failed for these notes.").arg(program),
                                explanation,
                                {Text::tr("Check that the resampler writes wave files that this "
                                          "wavtool reads, or use the wavtool supplied with the "
                                          "resampler.")},
                                failures,
                            });
                            break;
                        }
                        case NoTrack:
                            result.push_back({
                                Text::tr("The wavtool %1 wrote no track.").arg(program),
                                output.isEmpty()
                                    ? Text::tr("The wavtool produced no output.")
                                    : Text::tr("The output of the wavtool is: %1").arg(output),
                                {Text::tr("Check that the wavtool accepts the arguments of the "
                                          "wavtool of UTAU.")},
                                failures,
                            });
                            break;
                        case ExitCode:
                            result.push_back({
                                Text::tr("The resampler %1 ended with the exit code %2.")
                                    .arg(program)
                                    .arg(first.run->exitCode),
                                output.isEmpty()
                                    ? Text::tr("The resampler produced no output.")
                                    : Text::tr("The output of the resampler is: %1").arg(output),
                                resamplerRemedies,
                                failures,
                            });
                            break;
                        case NoFragment:
                            result.push_back({
                                Text::tr("The resampler %1 wrote no fragment.").arg(program),
                                output.isEmpty()
                                    ? Text::tr("The resampler produced no output.")
                                    : Text::tr("The output of the resampler is: %1").arg(output),
                                resamplerRemedies,
                                failures,
                            });
                            break;
                    }
                }
                return result;
            }
        };

    }

    BuiltinSynthDiagnosers::BuiltinSynthDiagnosers(SynthDiagnoserRegistry &registry) {
        m_registrations.push_back(SynthDiagnoserRegistry::Add<CodePage>(registry, "codePage", {}));
        m_registrations.push_back(SynthDiagnoserRegistry::Add<Start>(registry, "start", {}));
        m_registrations.push_back(
            SynthDiagnoserRegistry::Add<ExitStatus>(registry, "exitStatus", {}));
        m_registrations.push_back(SynthDiagnoserRegistry::Add<Timeout>(registry, "timeout", {}));
        m_registrations.push_back(SynthDiagnoserRegistry::Add<LongPath>(registry, "longPath", {}));
        m_registrations.push_back(SynthDiagnoserRegistry::Add<Output>(registry, "output", {}));
    }

    BuiltinSynthDiagnosers::~BuiltinSynthDiagnosers() = default;

}
