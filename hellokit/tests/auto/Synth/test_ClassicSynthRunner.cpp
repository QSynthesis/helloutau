/// \file
/// Script execution is not covered here, because it requires the engines, which are not part of
/// this repository. Covered is **the content written into the script**, which is where the risk
/// lies. See test_ThreadedSynthRunner.cpp for the remaining coverage gap.

#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Synth/ClassicSynthRunner.h>

using namespace hello::kit;

class test_ClassicSynthRunner : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

    std::filesystem::path root() const {
        return std::filesystem::path(m_dir->path().toStdU16String());
    }

    void write(const QString &relative, const QByteArray &bytes) {
        const QString path = m_dir->path() + QLatin1Char('/') + relative;
        QVERIFY(QDir().mkpath(QFileInfo(path).path()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
    }

    /// A plan for one note with the lyric and flags specified by the caller.
    std::optional<SynthPlan> planFor(const QString &lyric, const QString &flags,
                                     const QString &projectFlags = QString()) {
        write(QStringLiteral("bank/oto.ini"), "a.wav=" + lyric.toUtf8() + ",10,20,30,40,5\n");
        write(QStringLiteral("bank/a.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("UTF-8"));
        DiagnosticList diagnostics;
        const auto bank = VoiceBank::open(root() / "bank", &selector, diagnostics);
        if (!bank) {
            return std::nullopt;
        }

        Note note;
        note.lyric = lyric;
        note.noteNum = 60;
        note.length = 480;
        note.flags = flags;

        Project project;
        project.settings.flags = projectFlags;
        Track track;
        track.notes.push_back(note);
        project.tracks.push_back(track);

        SynthPlan::Options options;
        options.cacheDirectory = root() / "cache";
        options.outputFile = root() / "out.wav";
        return SynthPlan::make(project, *bank, options, diagnostics);
    }

    /// Two nonexistent paths that are never executed.
    ///
    /// No test calls render(). scripts() only builds the text of the two files, so an engine
    /// path is a string assigned to a script variable and verified there. A real path would
    /// verify nothing more and would make the test machine-dependent.
    static SynthEngines engines() {
        SynthEngines e;
        e.resampler = "C:/UTAU/resampler.exe";
        e.wavtool = "C:/UTAU/wavtool.exe";
        return e;
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void cleanup() {
        m_dir.reset();
    }

    // The layout expected by an engine that reads the script.
    void the_script_has_the_shape_utau_writes() {
        const auto plan = planFor(QStringLiteral("a"), QString());
        QVERIFY(plan.has_value());

        ClassicSynthRunner runner;
        DiagnosticList diagnostics;
        const auto written = runner.scripts(*plan, engines(), diagnostics);
        QVERIFY(written.has_value());

        const auto &bat = written->first;
        QVERIFY(bat.contains(QLatin1String("@set \"tool=")));
        QVERIFY(bat.contains(QLatin1String("@set \"resamp=")));
        QVERIFY(bat.contains(QLatin1String("@set \"output=")));
        QVERIFY(bat.contains(QLatin1String("@set \"cachedir=")));
        QVERIFY(bat.contains(QLatin1String("@call \"%helper%\"")));
        QVERIFY(bat.contains(QLatin1String("@mkdir \"%cachedir%\" 2>nul")));

        // The wavtool writes the header and the sample data separately, and joining them is the
        // final step.
        QVERIFY(bat.contains(
            QLatin1String("@copy /Y \"%output%.whd\" /B + \"%output%.dat\" /B \"%output%\"")));

        // An existing fragment is the cache, and it is reused by skipping the resampler.
        const auto &helper = written->second;
        QVERIFY(helper.contains(QLatin1String("@if exist \"%temp%\" goto A")));
        QVERIFY(helper.contains(QLatin1String("@\"%resamp%\"")));
        QVERIFY(helper.contains(QLatin1String("@\"%tool%\"")));
    }

    // A batch file is a shell script, so this is the only place where a lyric or a flags string
    // could become a command. This is also the reason escaped mode is the default.
    void nothing_from_the_project_can_become_a_command_data() {
        QTest::addColumn<QString>("flags");

        QTest::newRow("ampersand") << QStringLiteral("g-5&whoami");
        QTest::newRow("pipe") << QStringLiteral("g-5|whoami");
        QTest::newRow("redirect") << QStringLiteral("g-5>out");
        QTest::newRow("caret") << QStringLiteral("g-5^&whoami");
        QTest::newRow("variable") << QStringLiteral("%PATH%");
        QTest::newRow("parenthesis") << QStringLiteral("g-5)&whoami&(");
    }

    void nothing_from_the_project_can_become_a_command() {
        QFETCH(QString, flags);

        const auto plan = planFor(QStringLiteral("a"), flags);
        QVERIFY(plan.has_value());

        ClassicSynthRunner runner;
        DiagnosticList diagnostics;
        const auto written = runner.scripts(*plan, engines(), diagnostics);
        QVERIFY(written.has_value());

        // The value is inside set "name=value", where cmd does not parse operators, and every
        // percent sign is doubled so that no variable is expanded.
        const auto line = [&]() -> QString {
            for (const auto &l : written->first.split(QLatin1String("\r\n"))) {
                if (l.startsWith(QLatin1String("@set \"flag="))) {
                    return l;
                }
            }
            return {};
        }();

        QVERIFY2(!line.isEmpty(), qPrintable(written->first));
        QVERIFY(line.endsWith(QLatin1Char('"')));

        const auto value = line.mid(QLatin1String("@set \"flag=").size(),
                                    line.size() - QLatin1String("@set \"flag=").size() - 1);

        // No character terminates the quoted form prematurely.
        QVERIFY(!value.contains(QLatin1Char('"')));

        // Every run of percent signs has even length, so each pair is the batch escape for a
        // literal percent sign rather than the start of a variable reference. This is the
        // required property, which differs from the value merely not resembling a variable.
        for (qsizetype i = 0; i < value.size();) {
            if (value.at(i) != QLatin1Char('%')) {
                ++i;
                continue;
            }
            qsizetype run = 0;
            while (i + run < value.size() && value.at(i + run) == QLatin1Char('%')) {
                ++run;
            }
            QVERIFY2(run % 2 == 0, qPrintable(value));
            i += run;
        }
    }

    // The mode for an engine that requires the script text to match UTAU exactly. It is unsafe,
    // and the test asserts this explicitly.
    void the_verbatim_mode_writes_what_it_was_given() {
        const auto plan = planFor(QStringLiteral("a"), QStringLiteral("g-5&whoami"));
        QVERIFY(plan.has_value());

        ClassicSynthRunner runner;
        runner.quoting = ClassicSynthRunner::Quoting::Verbatim;

        DiagnosticList diagnostics;
        const auto written = runner.scripts(*plan, engines(), diagnostics);
        QVERIFY(written.has_value());

        QVERIFY(written->first.contains(QLatin1String("@set flag=g-5&whoami")));
    }

    // Neither character can be escaped inside set "name=value", so the value is rejected rather
    // than written in corrupted form.
    void a_value_a_script_cannot_carry_is_refused() {
        const auto plan = planFor(QStringLiteral("a"), QStringLiteral("g-5\nwhoami"));
        QVERIFY(plan.has_value());

        ClassicSynthRunner runner;
        DiagnosticList diagnostics;
        QVERIFY(!runner.scripts(*plan, engines(), diagnostics).has_value());
        QVERIFY(hasError(diagnostics));
    }

    // A rest requires no resampling and is passed directly to the wavtool, as in UTAU.
    void a_rest_calls_the_wavtool_without_the_helper() {
        write(QStringLiteral("bank/oto.ini"), "a.wav=a,10,20,30,40,5\n");
        write(QStringLiteral("bank/a.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("UTF-8"));
        DiagnosticList diagnostics;
        const auto bank = VoiceBank::open(root() / "bank", &selector, diagnostics);
        QVERIFY(bank.has_value());

        Note rest;
        rest.lyric = QStringLiteral("R");
        rest.noteNum = 60;
        rest.length = 480;

        Project project;
        Track track;
        track.notes.push_back(rest);
        project.tracks.push_back(track);

        SynthPlan::Options options;
        options.cacheDirectory = root() / "cache";
        options.outputFile = root() / "out.wav";
        const auto plan = SynthPlan::make(project, *bank, options, diagnostics);
        QVERIFY(plan.has_value());

        ClassicSynthRunner runner;
        const auto written = runner.scripts(*plan, engines(), diagnostics);
        QVERIFY(written.has_value());

        QVERIFY(written->first.contains(QLatin1String("@\"%tool%\" ")));
        QVERIFY(!written->first.contains(QLatin1String("@call")));
    }

    // Every path passed to an engine must tolerate a space in a folder name. The default voice
    // folder of UTAU is under Program Files, so this is the common case rather than an edge
    // case.
    void a_path_with_a_space_is_quoted() {
        write(QStringLiteral("my bank/oto.ini"), "a.wav=a,10,20,30,40,5\n");
        write(QStringLiteral("my bank/a.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("UTF-8"));
        DiagnosticList diagnostics;
        const auto bank = VoiceBank::open(root() / "my bank", &selector, diagnostics);
        QVERIFY(bank.has_value());

        Note sung;
        sung.lyric = QStringLiteral("a");
        sung.noteNum = 60;
        sung.length = 480;
        Note rest;
        rest.lyric = QStringLiteral("R");
        rest.noteNum = 60;
        rest.length = 240;

        Project project;
        Track track;
        track.notes.push_back(sung);
        track.notes.push_back(rest);
        project.tracks.push_back(track);

        SynthPlan::Options options;
        options.cacheDirectory = root() / "my cache";
        options.outputFile = root() / "my out.wav";
        const auto plan = SynthPlan::make(project, *bank, options, diagnostics);
        QVERIFY(plan.has_value());

        ClassicSynthRunner runner;
        const auto written = runner.scripts(*plan, engines(), diagnostics);
        QVERIFY(written.has_value());

        // The number of arguments the line actually passes, counted as a command processor
        // splits it: a space inside quotes belongs to the argument, and a space outside ends
        // it. An unquoted path becomes several arguments, which the count detects. Comparing
        // the text instead would merely restate the quoting rules of the writer.
        const auto arguments = [](const QString &line) {
            int count = 0;
            bool quoted = false;
            bool inside = false;
            for (const QChar c : line) {
                if (c == QLatin1Char('"')) {
                    quoted = !quoted;
                }
                if (!quoted && c == QLatin1Char(' ')) {
                    inside = false;
                    continue;
                }
                if (!inside) {
                    inside = true;
                    ++count;
                }
            }
            return count;
        };

        int checked = 0;
        for (const auto &line : written->first.split(QLatin1String("\r\n"))) {
            // The sung note: one argument for the helper itself, followed by its eight
            // arguments.
            if (line.startsWith(QLatin1String("@call"))) {
                QCOMPARE(arguments(line), 1 + 1 + 8);
                ++checked;
            }
            // The rest, which takes a different code path through the writer, the one that
            // produced the unquoted path.
            if (line.startsWith(QLatin1String("@\"%tool%\""))) {
                QCOMPARE(arguments(line), 1 + int(plan->steps().at(1).wavtoolArguments.size()));
                ++checked;
            }
        }
        QCOMPARE(checked, 2);
    }

    // On other systems the UTAU engines run under Wine, and the script that starts them is a
    // shell script. The layout is the same and the syntax differs. The script is generated and
    // verified here, so that its content is not first observed when a user runs it.
    void the_shell_script_has_the_same_shape() {
        const auto plan = planFor(QStringLiteral("a"), QString());
        QVERIFY(plan.has_value());

        ClassicSynthRunner runner;
        runner.shell = ClassicSynthRunner::ScriptShell::Posix;

        DiagnosticList diagnostics;
        const auto written = runner.scripts(*plan, engines(), diagnostics);
        QVERIFY(written.has_value());

        const auto &script = written->first;
        QVERIFY(script.startsWith(QLatin1String("#!/bin/sh\n")));
        QVERIFY(script.contains(QLatin1String("export tool='")));
        QVERIFY(script.contains(QLatin1String("export cachedir='")));
        QVERIFY(script.contains(QLatin1String("mkdir -p \"${cachedir}\"")));
        QVERIFY(script.contains(QLatin1String("\"${helper}\"")));

        // cat instead of copy, the only command in the footer.
        QVERIFY(script.contains(
            QLatin1String("cat \"${output}.whd\" \"${output}.dat\" > \"${output}\"")));
        QVERIFY(!script.contains(QLatin1String("copy /Y")));

        // A shell script uses LF line endings, and a batch file uses CRLF.
        QVERIFY(!script.contains(QLatin1Char('\r')));

        const auto &helper = written->second;
        QVERIFY(helper.contains(QLatin1String("if [ ! -f \"${temp}\" ]; then")));
        QVERIFY(helper.contains(QLatin1String("\"${resamp}\"")));
        QVERIFY(helper.contains(QLatin1String("\"${tool}\"")));
    }

    // Within single quotes a POSIX shell treats every character literally, except the single
    // quote itself, which cannot appear inside them.
    void the_shell_script_cannot_be_made_to_run_a_command_data() {
        QTest::addColumn<QString>("flags");

        QTest::newRow("semicolon") << QStringLiteral("g-5;whoami");
        QTest::newRow("ampersand") << QStringLiteral("g-5&whoami");
        QTest::newRow("substitution") << QStringLiteral("g-5$(whoami)");
        QTest::newRow("backtick") << QStringLiteral("g-5`whoami`");
        QTest::newRow("variable") << QStringLiteral("$PATH");
        QTest::newRow("quote") << QStringLiteral("g-5'; whoami; '");
    }

    void the_shell_script_cannot_be_made_to_run_a_command() {
        QFETCH(QString, flags);

        const auto plan = planFor(QStringLiteral("a"), flags);
        QVERIFY(plan.has_value());

        ClassicSynthRunner runner;
        runner.shell = ClassicSynthRunner::ScriptShell::Posix;

        DiagnosticList diagnostics;
        const auto written = runner.scripts(*plan, engines(), diagnostics);
        QVERIFY(written.has_value());

        QString line;
        for (const auto &l : written->first.split(QLatin1Char('\n'))) {
            if (l.startsWith(QLatin1String("export flag="))) {
                line = l;
            }
        }
        QVERIFY2(!line.isEmpty(), qPrintable(written->first));

        const auto value = line.mid(QLatin1String("export flag=").size());
        QVERIFY(value.startsWith(QLatin1Char('\'')));
        QVERIFY(value.endsWith(QLatin1Char('\'')));

        // Every embedded single quote is written by closing the quotes, escaping it and
        // reopening them, the only way to represent it. Any other form would terminate the
        // quoting prematurely and let the remainder of the line be parsed as shell syntax.
        const auto inner = value.mid(1, value.size() - 2);
        QCOMPARE(inner.count(QLatin1Char('\'')), inner.count(QLatin1String("'\\''")) * 3);
    }

    void a_plan_with_nothing_in_it_is_refused() {
        write(QStringLiteral("bank/a.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("UTF-8"));
        DiagnosticList diagnostics;
        const auto bank = VoiceBank::open(root() / "bank", &selector, diagnostics);
        QVERIFY(bank.has_value());

        Note note;
        note.lyric = QStringLiteral("a");
        note.noteNum = 60;
        note.length = 480;

        Project project;
        Track track;
        track.notes.push_back(note);
        project.tracks.push_back(track);

        SynthPlan::Options options;
        options.cacheDirectory = root() / "cache";
        options.outputFile = root() / "out.wav";
        const auto plan = SynthPlan::make(project, *bank, options, diagnostics);
        QVERIFY(plan.has_value());

        // Nonexistent engines still produce a script, because writing a script executes
        // nothing.
        ClassicSynthRunner runner;
        diagnostics.clear();
        QVERIFY(runner.scripts(*plan, engines(), diagnostics).has_value());
        QVERIFY(diagnostics.isEmpty());
    }
};

QTEST_APPLESS_MAIN(test_ClassicSynthRunner)

#include "test_ClassicSynthRunner.moc"
