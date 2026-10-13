#include <QtTest/QTest>

#include <hellokit/Support/TextCodec.h>
#include <hellokit/Synth/BuiltinSynthDiagnosers.h>
#include <hellokit/Synth/SynthDiagnosers.h>

using namespace hello::kit;

namespace {

    SynthToolRun runOf(bool started, int exitCode = 0, bool timedOut = false) {
        SynthToolRun run;
        run.started = started;
        run.exitCode = exitCode;
        run.timedOut = timedOut;
        return run;
    }

    SynthFailure failureOf(SynthFailure::Stage stage, std::optional<SynthToolRun> run,
                           const QStringList &arguments = {QStringLiteral("in.wav"),
                                                           QStringLiteral("out.wav")}) {
        SynthFailure failure;
        failure.stage = stage;
        failure.program = "C:/tools/resampler.exe";
        failure.arguments = arguments;
        failure.run = run;
        failure.file = "C:/cache/out.wav";
        return failure;
    }

}

class test_BuiltinSynthDiagnosers : public QObject {
    Q_OBJECT

private:
    SynthDiagnosers m_diagnosers;
    BuiltinSynthDiagnosers m_builtins{m_diagnosers.registry()};

    QList<SynthDiagnosis> diagnose(const QList<SynthFailure> &failures) const {
        SynthDiagnosisContext context;
        context.failures = failures;
        return m_diagnosers.diagnose(context);
    }

private Q_SLOTS:
    void the_builtins_are_registered() {
        QStringList ids;
        for (const auto diagnoser : m_diagnosers.diagnosers()) {
            ids.push_back(diagnoser->id());
        }
        QCOMPARE(ids,
                 QStringList({"codePage", "start", "exitStatus", "timeout", "longPath", "output"}));
    }

    void a_tool_that_did_not_start_is_diagnosed_once() {
        const auto diagnoses = diagnose({failureOf(SynthFailure::Resampler, runOf(false)),
                                         failureOf(SynthFailure::Resampler, runOf(false))});
        QCOMPARE(diagnoses.size(), 1);
        QCOMPARE(diagnoses.at(0).failures, (QList<int>{0, 1}));
        QVERIFY(diagnoses.at(0).cause.contains(QStringLiteral("resampler.exe")));
        QVERIFY(diagnoses.at(0).cause.contains(QStringLiteral("started")));
        QVERIFY(!diagnoses.at(0).remedies.isEmpty());
    }

    void the_exit_statuses_of_windows_are_explained() {
#ifndef Q_OS_WINDOWS
        QSKIP("The statuses are those of Windows.");
#endif
        const auto diagnoses =
            diagnose({failureOf(SynthFailure::Resampler, runOf(true, int(0xC0000135))),
                      failureOf(SynthFailure::Resampler, runOf(true, int(0xC000007B))),
                      failureOf(SynthFailure::Resampler, runOf(true, int(0xC0000005))),
                      failureOf(SynthFailure::Resampler, runOf(true, 3))});
        QCOMPARE(diagnoses.size(), 4);
        QVERIFY(diagnoses.at(0).explanation.contains(QStringLiteral("0xC0000135")));
        QVERIFY(diagnoses.at(1).explanation.contains(QStringLiteral("0xC000007B")));
        QVERIFY(diagnoses.at(2).explanation.contains(QStringLiteral("0xC0000005")));
        // Another exit code is left to the diagnoser of the output.
        QCOMPARE(diagnoses.at(3).failures, QList<int>{3});
        QVERIFY(diagnoses.at(3).cause.contains(QStringLiteral("3")));
    }

    void a_timeout_is_diagnosed() {
        const auto diagnoses = diagnose({failureOf(SynthFailure::Resampler, runOf(true, 0, true))});
        QCOMPARE(diagnoses.size(), 1);
        QVERIFY(diagnoses.at(0).cause.contains(QStringLiteral("time limit")));
    }

    void a_path_outside_the_code_page_is_diagnosed() {
#ifndef Q_OS_WINDOWS
        QSKIP("The code page is that of Windows.");
#endif
        // A character that the code page of this system cannot represent, the first of
        // several candidates
        const TextCodec codec(TextCodec::systemName());
        QString character;
        for (const auto candidate : {u'\u0e01', u'\u05d0', u'\u0915', u'\u4f60', u'\u3042'}) {
            if (!codec.canEncode(QString(QChar(candidate)))) {
                character = QChar(candidate);
                break;
            }
        }
        if (character.isEmpty()) {
            QSKIP("The code page of this system represents every candidate.");
        }
        const auto path = QStringLiteral("C:/voice/") + character + QStringLiteral("/a.wav");
        const auto diagnoses = diagnose(
            {failureOf(SynthFailure::Resampler, runOf(true, 1), {path, QStringLiteral("out.wav")}),
             failureOf(SynthFailure::Resampler, runOf(true, 1))});
        QCOMPARE(diagnoses.size(), 2);
        QCOMPARE(diagnoses.at(0).failures, QList<int>{0});
        QVERIFY(diagnoses.at(0).explanation.contains(path));
        QCOMPARE(diagnoses.at(1).failures, QList<int>{1});
    }

    void a_long_path_is_diagnosed() {
#ifndef Q_OS_WINDOWS
        QSKIP("The limit is that of Windows.");
#endif
        const auto path = QStringLiteral("C:/") + QString(300, u'a') + QStringLiteral(".wav");
        const auto diagnoses = diagnose(
            {failureOf(SynthFailure::Resampler, runOf(true, 1), {path, QStringLiteral("o.wav")})});
        QCOMPARE(diagnoses.size(), 1);
        QVERIFY(diagnoses.at(0).cause.contains(QStringLiteral("260")));
    }

    // A wave file that realtime playback cannot read is distinguished from a file that is not
    // a wave file.
    void a_wave_file_that_cannot_be_read_is_told_apart() {
        auto notWave = failureOf(SynthFailure::Fragment, runOf(true));
        notWave.fileHead = "This is a dummy file";
        auto unreadable = failureOf(SynthFailure::Fragment, runOf(true));
        unreadable.fileHead = QByteArray("RIFF\0\0\0\0WAVEfmt ", 16);
        const auto diagnoses = diagnose({notWave, unreadable});
        QCOMPARE(diagnoses.size(), 2);
        QCOMPARE(diagnoses.at(0).failures, QList<int>{0});
        QCOMPARE(diagnoses.at(1).failures, QList<int>{1});
        QVERIFY(diagnoses.at(0).cause != diagnoses.at(1).cause);
    }

    // A failed wavtool call of a note names the fragment if it is not a wave file.
    void a_failed_wavtool_call_of_a_note_names_its_fragment() {
        auto failure = failureOf(SynthFailure::Wavtool, runOf(true, -1));
        failure.noteIndex = 3;
        failure.fileSize = 180;
        failure.fileHead = "This is a dummy file";
        const auto diagnoses = diagnose({failure});
        QCOMPARE(diagnoses.size(), 1);
        QCOMPARE(diagnoses.at(0).failures, QList<int>{0});
        QVERIFY(diagnoses.at(0).explanation.contains(QStringLiteral("-1")));
        QVERIFY(diagnoses.at(0).explanation.contains(QStringLiteral("This is a dummy file")));

        failure.fileHead = QByteArray("RIFF\0\0\0\0WAVEfmt ", 16);
        QVERIFY(!diagnose({failure}).at(0).explanation.contains(QStringLiteral("RIFF")));

        // A record without the result of the call, which a host may pass
        failure.run = std::nullopt;
        const auto withoutRun = diagnose({failure});
        QCOMPARE(withoutRun.size(), 1);
        QVERIFY(!withoutRun.at(0).explanation.isEmpty());
    }

    // A fragment that exists although the classic wavtool wrote no track is left to a diagnoser
    // that recognizes its content, even with a path longer than the limit of Windows.
    void an_unconfirmed_fragment_is_left_to_other_diagnosers() {
        const auto track = failureOf(SynthFailure::Wavtool, std::nullopt);
        auto fragment =
            failureOf(SynthFailure::Unconfirmed, std::nullopt,
                      {QStringLiteral("C:/") + QString(300, u'a') + QStringLiteral(".wav"),
                       QStringLiteral("o.wav")});
        fragment.noteIndex = 0;
        fragment.fileHead = "This is a dummy file";
        const auto diagnoses = diagnose({track, fragment});
        QCOMPARE(diagnoses.size(), 1);
        QCOMPARE(diagnoses.at(0).failures, QList<int>{0});
    }

    void the_remaining_failures_are_grouped_by_kind() {
        auto notWave = failureOf(SynthFailure::Fragment, runOf(true));
        notWave.fileHead = "This is a dummy file\x01";
        const auto diagnoses = diagnose({
            notWave,
            failureOf(SynthFailure::Resampler, runOf(true, 0)),
            failureOf(SynthFailure::Resampler, runOf(true, 2)),
            failureOf(SynthFailure::Wavtool, runOf(true, 0)),
            failureOf(SynthFailure::Resampler, std::nullopt),
            failureOf(SynthFailure::Resampler, runOf(true, 2)),
        });
        QCOMPARE(diagnoses.size(), 4);
        QCOMPARE(diagnoses.at(0).failures, QList<int>{0});
        QVERIFY(diagnoses.at(0).explanation.contains(QStringLiteral("This is a dummy file.")));
        // No fragment, whether the exit code is 0 or unknown
        QCOMPARE(diagnoses.at(1).failures, (QList<int>{1, 4}));
        QCOMPARE(diagnoses.at(2).failures, (QList<int>{2, 5}));
        QCOMPARE(diagnoses.at(3).failures, QList<int>{3});
    }

    // In the generation of frequency maps, a resampler that ended normally without a map is
    // diagnosed with the format, and the remedies concern the audio files and the voice bank
    // instead of the notes and the project.
    void the_generation_of_frequency_maps_is_diagnosed() {
        SynthDiagnosisContext context;
        context.operation = SynthDiagnosisContext::FrequencyMapGeneration;
        context.frequencyFormat = QStringLiteral("mrq (moresampler)");
        context.failures = {failureOf(SynthFailure::FrequencyMap, runOf(true, 0)),
                            failureOf(SynthFailure::Resampler, runOf(true, 2)),
                            failureOf(SynthFailure::FrequencyMap, runOf(true, 0)),
                            failureOf(SynthFailure::Resampler, runOf(true, 0, true))};
        const auto diagnoses = m_diagnosers.diagnose(context);
        const auto audioFiles = QStringLiteral("Check these audio files in the voice bank editor.");
        QCOMPARE(diagnoses.size(), 3);
        QCOMPARE(diagnoses.at(0).failures, QList<int>{3});
        QVERIFY(diagnoses.at(0).explanation.contains(QStringLiteral("audio file")));
        QCOMPARE(diagnoses.at(1).failures, (QList<int>{0, 2}));
        QVERIFY(diagnoses.at(1).cause.contains(QStringLiteral("mrq (moresampler)")));
        QVERIFY(diagnoses.at(1).remedies.contains(audioFiles));
        QCOMPARE(diagnoses.at(2).failures, QList<int>{1});
        QCOMPARE(diagnoses.at(2).remedies.value(0), audioFiles);

        // The same failure of a render concerns the notes.
        const auto render = diagnose({failureOf(SynthFailure::Resampler, runOf(true, 2))});
        QVERIFY(render.value(0).remedies.value(0).contains(QStringLiteral("notes")));

#ifdef Q_OS_WINDOWS
        context.failures = {
            failureOf(SynthFailure::FrequencyMap, runOf(true, 0),
                      {QStringLiteral("C:/") + QString(300, u'a') + QStringLiteral(".wav"),
                       QStringLiteral("o.wav")})};
        QCOMPARE(m_diagnosers.diagnose(context).value(0).remedies,
                 QStringList{QStringLiteral("Move the voice bank to a shorter path.")});
#endif
    }
};

QTEST_APPLESS_MAIN(test_BuiltinSynthDiagnosers)

#include "test_BuiltinSynthDiagnosers.moc"
