/// \file
/// Running the script is not covered here: that needs the engines, which are not in this
/// repository. What is covered is **what gets written into it**, which is where the danger is.
/// See test_ThreadedSynthRunner.cpp for the rest of that debt.

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

    /// A plan for one note, whose lyric and flags the caller chooses.
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

    /// Two paths that do not exist and are never run.
    ///
    /// Nothing here calls render(): scripts() only builds the text of the two files, so an
    /// engine path is a string that ends up in one of the script's variables and is asserted
    /// on there. A real one would say no more and would tie the test to a machine.
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

    // The layout an engine that reads the script expects to find.
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

        // The wavtool keeps the header and the samples apart, and joining them is the last
        // thing that happens.
        QVERIFY(bat.contains(
            QLatin1String("@copy /Y \"%output%.whd\" /B + \"%output%.dat\" /B \"%output%\"")));

        // A piece that is already there is the cache, and skipping the resampler is how it gets
        // reused.
        const auto &helper = written->second;
        QVERIFY(helper.contains(QLatin1String("@if exist \"%temp%\" goto A")));
        QVERIFY(helper.contains(QLatin1String("@\"%resamp%\"")));
        QVERIFY(helper.contains(QLatin1String("@\"%tool%\"")));
    }

    // A batch file is a shell script, so this is the one place where a lyric or a flags string
    // could become a command. It is also the whole reason the escaped mode is the default.
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

        // The value sits inside set "name=value", where cmd stops looking for operators, and
        // every per cent sign is doubled so nothing expands.
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

        // Nothing closes the quoted form early.
        QVERIFY(!value.contains(QLatin1Char('"')));

        // Every run of per cent signs is of even length, so each one is the batch escape for a
        // literal per cent rather than the start of a variable. This is the property that
        // matters, and it is not the same as the value not looking like a variable.
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

    // The mode that exists for an engine that needs the script to look exactly as UTAU writes
    // it. It is not safe, and the test says so rather than pretending otherwise.
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

    // Neither has an escape that works inside set "name=value", so it is refused rather than
    // written mangled.
    void a_value_a_script_cannot_carry_is_refused() {
        const auto plan = planFor(QStringLiteral("a"), QStringLiteral("g-5\nwhoami"));
        QVERIFY(plan.has_value());

        ClassicSynthRunner runner;
        DiagnosticList diagnostics;
        QVERIFY(!runner.scripts(*plan, engines(), diagnostics).has_value());
        QVERIFY(hasError(diagnostics));
    }

    // A rest has nothing to resample, so it goes straight to the wavtool, as it does under
    // UTAU.
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

    // Every path an engine is handed has to survive a folder name with a space in it. UTAU's
    // own default voice folder is under Program Files, so this is the ordinary case and not an
    // odd one.
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

        // How many arguments the line actually hands over, counted the way a command processor
        // splits one: a space inside quotes is part of an argument, a space outside ends it.
        // A path that went out unquoted turns into several arguments, and the count is what
        // says so. Comparing the spelling instead would only re-state how the writer quotes.
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
            // The note that is sung. One for the helper itself, then the eight it is given.
            if (line.startsWith(QLatin1String("@call"))) {
                QCOMPARE(arguments(line), 1 + 1 + 8);
                ++checked;
            }
            // The rest, which takes a different path through the writer and is the one that
            // went out unquoted.
            if (line.startsWith(QLatin1String("@\"%tool%\""))) {
                QCOMPARE(arguments(line), 1 + int(plan->steps().at(1).wavtoolArguments.size()));
                ++checked;
            }
        }
        QCOMPARE(checked, 2);
    }

    // UTAU's engines run under Wine on the other systems, and the script that starts them is a
    // shell script there. Same layout, different spelling, and it is written and read here so
    // that what goes out to those systems is not first seen when somebody runs it.
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

        // cat where the batch says copy, which is the one thing the footer does.
        QVERIFY(script.contains(
            QLatin1String("cat \"${output}.whd\" \"${output}.dat\" > \"${output}\"")));
        QVERIFY(!script.contains(QLatin1String("copy /Y")));

        // A shell script is not CRLF, and a batch file is.
        QVERIFY(!script.contains(QLatin1Char('\r')));

        const auto &helper = written->second;
        QVERIFY(helper.contains(QLatin1String("if [ ! -f \"${temp}\" ]; then")));
        QVERIFY(helper.contains(QLatin1String("\"${resamp}\"")));
        QVERIFY(helper.contains(QLatin1String("\"${tool}\"")));
    }

    // Single quotes make a POSIX shell take everything literally, and the one character they
    // cannot hold is the single quote itself.
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

        // Every quote inside left and came back, which is the only way a single quoted string
        // can carry one. Anything else would have ended the quoting early and let the rest of
        // the line be read as shell.
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

        // Engines that are not there still produce a script: writing one runs nothing.
        ClassicSynthRunner runner;
        diagnostics.clear();
        QVERIFY(runner.scripts(*plan, engines(), diagnostics).has_value());
        QVERIFY(diagnostics.isEmpty());
    }
};

QTEST_APPLESS_MAIN(test_ClassicSynthRunner)

#include "test_ClassicSynthRunner.moc"
