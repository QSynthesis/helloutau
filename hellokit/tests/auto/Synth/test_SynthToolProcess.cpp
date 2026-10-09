#ifdef _WIN32
#  include <fcntl.h>
#  include <io.h>
#endif

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>

#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QStringList>
#include <QtCore/QTemporaryDir>
#include <QtCore/QRegularExpression>
#include <QtTest/QTest>

#include <hellokit/Synth/SynthToolProcess.h>

using namespace hello::kit;

namespace {

    /// The argument that makes this binary act as the synth tool under test.
    constexpr char echoArguments[] = "--echo-arguments";

    /// The argument that makes it hang, for the time limit case.
    constexpr char sleepForever[] = "--sleep-forever";

    /// The argument that makes it fail, for the exit code case.
    constexpr char failWith[] = "--fail-with";

    /// The argument that makes it write the first file that follows at once and the second a
    /// second later, for the case of a synth tool started by a script.
    constexpr char writeLater[] = "--write-later";

}

class test_SynthToolProcess : public QObject {
    Q_OBJECT

private:
    /// This binary, acting as a substitute synth tool.
    ///
    /// The substitute only needs to report its arguments, and the one program certain to exist
    /// during a test is the test itself. A separate helper program would only add maintenance.
    static std::filesystem::path self() {
        return std::filesystem::path(QCoreApplication::applicationFilePath().toStdU16String());
    }

    static SynthToolRun run(const QStringList &arguments, DiagnosticList &diagnostics,
                            int timeout = 30000) {
        SynthToolProcess synthTool;
        synthTool.timeout = timeout;
        return synthTool.run(self(), arguments, diagnostics);
    }

private Q_SLOTS:
    void it_runs_a_program_and_reports_what_it_printed() {
        DiagnosticList diagnostics;
        const auto result =
            run({QLatin1String(echoArguments), QStringLiteral("hello")}, diagnostics);

        QVERIFY(result.started);
        QVERIFY(result.succeeded());
        QVERIFY(diagnostics.isEmpty());
        QCOMPARE(result.output.trimmed(), QStringLiteral("hello"));
    }

    void it_records_timestamped_output_with_the_selected_retention() {
        const auto log = std::make_shared<SynthToolOutputLog>();
        log->setMode(SynthToolOutputLog::Latest);
        log->setLimit(1024 * 1024);
        SynthToolProcess synthTool(log);
        DiagnosticList diagnostics;
        QVERIFY(
            synthTool
                .run(self(), {QLatin1String(echoArguments), QStringLiteral("first")}, diagnostics)
                .succeeded());
        log->clear();
        QVERIFY(
            synthTool
                .run(self(), {QLatin1String(echoArguments), QStringLiteral("second")}, diagnostics)
                .succeeded());
        const auto text = log->text();
        QVERIFY(!text.contains(QStringLiteral("first")));
        QVERIFY(text.contains(QStringLiteral("second")));
        QVERIFY(text.contains(QRegularExpression(QStringLiteral(
            R"(\[\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3})"))));
    }

    // The reason this class exists. UTAU renders by writing a batch file, so a sample path or
    // a flags string containing & or a newline appends commands to it. Here each value must
    // arrive as a single argument with no other effect.
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

        // One argument in, one identical line out. A shell would have split it, expanded it,
        // or executed its second half.
        QCOMPARE(result.output, argument + QLatin1Char('\n'));
    }

    void several_arguments_stay_apart() {
        DiagnosticList diagnostics;
        const auto result = run({QLatin1String(echoArguments), QStringLiteral("one"),
                                 QStringLiteral("two three"), QString(), QStringLiteral("four")},
                                diagnostics);

        QVERIFY(result.started);
        // The empty argument counts, because a synth tool reads its arguments by position.
        QCOMPARE(result.output, QStringLiteral("one\ntwo three\n\nfour\n"));
    }

    void a_program_that_is_not_there_is_reported_rather_than_run() {
        DiagnosticList diagnostics;
        SynthToolProcess synthTool;
        const auto result = synthTool.run("nowhere/resampler.exe", {}, diagnostics);

        QVERIFY(!result.started);
        QVERIFY(!result.succeeded());
        QVERIFY(hasError(diagnostics));
    }

    void a_failing_synth_tool_reports_its_exit_code() {
        DiagnosticList diagnostics;
        const auto result = run({QLatin1String(failWith), QStringLiteral("3")}, diagnostics);

        QVERIFY(result.started);
        QCOMPARE(result.exitCode, 3);
        QVERIFY(!result.succeeded());
    }

    // Otherwise a synth tool that never returns would halt the render indefinitely, as in UTAU.
    void an_synth_tool_that_hangs_is_stopped() {
        DiagnosticList diagnostics;
        const auto result = run({QLatin1String(sleepForever)}, diagnostics, 500);

        QVERIFY(result.started);
        QVERIFY(result.timedOut);
        QVERIFY(!result.succeeded());
        QVERIFY(hasError(diagnostics));
    }

    // A cancelled script ends at once with the synth tool it started, which would otherwise write
    // its second file a second later. A kill of the script alone leaves the synth tool running, as
    // Popen.kill() of Python does. The script is cancelled once the synth tool runs, which writes
    // its first file. The console of the script shows briefly.
    void a_cancelled_script_ends_with_its_synth_tools() {
        QTemporaryDir dir;
        const auto started = dir.filePath(QStringLiteral("started"));
        const auto marker = dir.filePath(QStringLiteral("written"));
#ifdef _WIN32
        const auto script = dir.filePath(QStringLiteral("temp.bat"));
#else
        const auto script = dir.filePath(QStringLiteral("temp.sh"));
#endif
        {
            QFile file(script);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(QStringLiteral("\"%1\" %2 \"%3\" \"%4\"\n")
                           .arg(QString::fromStdU16String(self().u16string()),
                                QLatin1String(writeLater), started, marker)
                           .toUtf8());
        }
        QFile::setPermissions(script, QFile::permissions(script) | QFile::ExeOwner);

        // Cancelled once the synth tool runs, or after five seconds without it
        QElapsedTimer elapsed;
        elapsed.start();
        qint64 cancelledAt = -1;
        SynthToolProcess synthTool;
        DiagnosticList diagnostics;
        const auto result = synthTool.runScript(
            std::filesystem::path(script.toStdU16String()), diagnostics,
            [&elapsed, &cancelledAt, &started] {
                if (cancelledAt < 0 && (QFile::exists(started) || elapsed.elapsed() > 5000)) {
                    cancelledAt = elapsed.elapsed();
                }
                return cancelledAt >= 0;
            });
        QVERIFY(QFile::exists(started));
        QVERIFY(result.started);
        QVERIFY(result.cancelled);
        QVERIFY2(elapsed.elapsed() - cancelledAt < 500,
                 qPrintable(QString::number(elapsed.elapsed() - cancelledAt)));
        QTest::qWait(1500);
        QVERIFY(!QFile::exists(marker));
    }
};

int main(int argc, char *argv[]) {
    // Entry point when acting as the synth tool under test. This code does not use Qt, so it does
    // not interfere with the measured behavior.
    if (argc >= 2) {
        const std::string mode = argv[1];
        if (mode == echoArguments) {
#ifdef _WIN32
            // Text mode would convert every line feed to CRLF, and arguments containing line
            // feeds are the subject of these cases. The received bytes must be echoed unchanged.
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
        if (mode == writeLater && argc >= 4) {
            std::ofstream(argv[2]).put('x');
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::ofstream(argv[3]).put('x');
            return 0;
        }
        if (mode == sleepForever) {
            for (;;) {
                QTest::qSleep(1000);
            }
        }
    }

    QCoreApplication app(argc, argv);
    test_SynthToolProcess object;
    return QTest::qExec(&object, argc, argv);
}

#include "test_SynthToolProcess.moc"
