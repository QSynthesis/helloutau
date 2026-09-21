#include <cstdio>
#include <cstring>
#include <string>

#ifdef _WIN32
#  include <fcntl.h>
#  include <io.h>
#endif

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QStringList>
#include <QtTest/QTest>

#include <hellokit/Synth/EngineProcess.h>

using namespace hello::kit;

namespace {

    /// The argument that turns this binary into the engine under test.
    constexpr char echoArguments[] = "--echo-arguments";

    /// The one that makes it hang, for the timeout case.
    constexpr char sleepForever[] = "--sleep-forever";

    /// The one that makes it fail, for the exit code case.
    constexpr char failWith[] = "--fail-with";

}

class test_EngineProcess : public QObject {
    Q_OBJECT

private:
    /// This binary, standing in for an engine.
    ///
    /// An engine is a program that reports what it was handed, and the one program a test can be
    /// sure exists is the test. Building a helper for it would only add a second thing to keep
    /// working.
    static std::filesystem::path self() {
        return std::filesystem::path(QCoreApplication::applicationFilePath().toStdU16String());
    }

    static EngineRun run(const QStringList &arguments, DiagnosticList &diagnostics,
                         int timeout = 30000) {
        EngineProcess engine;
        engine.timeout = timeout;
        return engine.run(self(), arguments, diagnostics);
    }

private Q_SLOTS:
    void it_runs_a_program_and_reports_what_it_printed() {
        DiagnosticList diagnostics;
        const auto result = run({QLatin1String(echoArguments), QStringLiteral("hello")},
                                diagnostics);

        QVERIFY(result.started);
        QVERIFY(result.succeeded());
        QVERIFY(diagnostics.isEmpty());
        QCOMPARE(result.output.trimmed(), QStringLiteral("hello"));
    }

    // The whole reason this class exists. UTAU renders by writing a batch file, so a sample
    // path or a flags string holding & or a newline appends commands to it. Here each of them
    // has to arrive as one argument and nothing else may happen.
    void a_shell_metacharacter_stays_inside_its_argument_data() {
        QTest::addColumn<QString>("argument");

        QTest::newRow("ampersand") << QStringLiteral("a & whoami");
        QTest::newRow("pipe") << QStringLiteral("a | whoami");
        QTest::newRow("semicolon") << QStringLiteral("a ; whoami");
        QTest::newRow("newline") << QStringLiteral("a\nwhoami");
        QTest::newRow("quote") << QStringLiteral("a \" b");
        QTest::newRow("backslash") << QStringLiteral("C:\\voice\\a.wav");
        QTest::newRow("trailing backslash") << QStringLiteral("C:\\voice\\");
        QTest::newRow("percent") << QStringLiteral("%PATH%");
        QTest::newRow("caret") << QStringLiteral("a ^ b");
        QTest::newRow("spaces") << QStringLiteral("  two  spaces  ");
        QTest::newRow("flags") << QStringLiteral("g-5Y0H0&B50");
    }

    void a_shell_metacharacter_stays_inside_its_argument() {
        QFETCH(QString, argument);

        DiagnosticList diagnostics;
        const auto result = run({QLatin1String(echoArguments), argument}, diagnostics);

        QVERIFY(result.started);
        QVERIFY(diagnostics.isEmpty());

        // One argument in, one line out, byte for byte. A shell would have split it, expanded
        // it, or run the second half.
        QCOMPARE(result.output, argument + QLatin1Char('\n'));
    }

    void several_arguments_stay_apart() {
        DiagnosticList diagnostics;
        const auto result = run({QLatin1String(echoArguments), QStringLiteral("one"),
                                 QStringLiteral("two three"), QString(), QStringLiteral("four")},
                                diagnostics);

        QVERIFY(result.started);
        // The empty one counts, since an engine reads its arguments by position.
        QCOMPARE(result.output, QStringLiteral("one\ntwo three\n\nfour\n"));
    }

    void a_program_that_is_not_there_is_reported_rather_than_run() {
        DiagnosticList diagnostics;
        EngineProcess engine;
        const auto result = engine.run("nowhere/resampler.exe", {}, diagnostics);

        QVERIFY(!result.started);
        QVERIFY(!result.succeeded());
        QVERIFY(hasError(diagnostics));
    }

    void a_failing_engine_reports_its_exit_code() {
        DiagnosticList diagnostics;
        const auto result = run({QLatin1String(failWith), QStringLiteral("3")}, diagnostics);

        QVERIFY(result.started);
        QCOMPARE(result.exitCode, 3);
        QVERIFY(!result.succeeded());
    }

    // An engine that never returns would otherwise stop the render for good, which is what UTAU
    // does.
    void an_engine_that_hangs_is_stopped() {
        DiagnosticList diagnostics;
        const auto result = run({QLatin1String(sleepForever)}, diagnostics, 500);

        QVERIFY(result.started);
        QVERIFY(result.timedOut);
        QVERIFY(!result.succeeded());
        QVERIFY(hasError(diagnostics));
    }
};

int main(int argc, char *argv[]) {
    // Re-entered as the engine under test. Nothing here uses Qt, so it stays out of the way of
    // whatever the test needs to measure.
    if (argc >= 2) {
        const std::string mode = argv[1];
        if (mode == echoArguments) {
#ifdef _WIN32
            // Text mode would turn every newline into CRLF, and an argument that carries one is
            // exactly what these cases are about. What arrived has to come back unchanged.
            _setmode(_fileno(stdout), _O_BINARY);
#endif
            for (int i = 2; i < argc; ++i) {
                std::fwrite(argv[i], 1, std::strlen(argv[i]), stdout);
                std::fputc('\n', stdout);
            }
            std::fflush(stdout);
            return 0;
        }
        if (mode == failWith) {
            return argc >= 3 ? std::atoi(argv[2]) : 1;
        }
        if (mode == sleepForever) {
            for (;;) {
                QTest::qSleep(1000);
            }
        }
    }

    QCoreApplication app(argc, argv);
    test_EngineProcess object;
    return QTest::qExec(&object, argc, argv);
}

#include "test_EngineProcess.moc"
