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
        failure.outputFile = "C:/cache/out.wav";
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
        // A character that the code page of this system cannot represent, whichever it is
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

    void the_remaining_failures_are_grouped_by_kind() {
        auto notWave = failureOf(SynthFailure::Fragment, runOf(true));
        notWave.outputHead = "This is a dummy file\x01";
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
};

QTEST_APPLESS_MAIN(test_BuiltinSynthDiagnosers)

#include "test_BuiltinSynthDiagnosers.moc"
