/// \file
/// Script execution is not covered here, because it requires the synth tools, which are not part of
/// this repository. Covered is **the content written into the script**, which is where the risk
/// lies. See test_ThreadedSynthRunner.cpp for the remaining coverage gap.

#include <fstream>
#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtCore/QThread>
#include <QtTest/QTest>

#include <hellokit/Support/TextCodec.h>
#include <hellokit/Synth/ClassicSynthRunner.h>
#include <hellokit/Synth/SynthToolProcess.h>
#include <hellokit/Synth/private/ShellSyntax_p.h>

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

    /// Runs no script, and records the time limit it was given.
    class StandIn : public SynthToolProcess {
    public:
        explicit StandIn(int *timeout) : m_timeout(timeout) {
        }

        SynthToolRun runScript(const std::filesystem::path &, DiagnosticList &,
                               const std::function<bool()> &) const override {
            *m_timeout = timeout;
            SynthToolRun run;
            run.started = true;
            return run;
        }

    private:
        int *m_timeout;
    };

    /// A runner whose script runs nothing.
    class StubbedRunner : public ClassicSynthRunner {
    public:
        mutable int timeout = 0;

        std::unique_ptr<SynthToolProcess> makeSynthToolProcess() const override {
            return std::make_unique<StandIn>(&timeout);
        }
    };

    /// Two nonexistent paths that are never executed.
    ///
    /// Only the stubbed runner calls render(). scripts() only builds the text of the two files, so
    /// a synth tool path is a string assigned to a script variable and verified there. A real path
    /// would verify nothing more and would make the test machine-dependent.
    static SynthTools synthTools() {
        SynthTools e;
        e.resampler = "C:/UTAU/resampler.exe";
        e.wavtool = "C:/UTAU/wavtool.exe";
        return e;
    }

    /// A plan for \a count notes, each with its own fragment.
    std::optional<SynthPlan> planOfNotes(int count) {
        write(QStringLiteral("bank/oto.ini"), "a.wav=a,10,20,30,40,5\n");
        write(QStringLiteral("bank/a.wav"), "RIFF");
        FixedCharsetSelector selector(QStringLiteral("UTF-8"));
        DiagnosticList diagnostics;
        const auto bank = VoiceBank::open(root() / "bank", &selector, diagnostics);
        if (!bank) {
            return std::nullopt;
        }
        Track track;
        for (int i = 0; i < count; ++i) {
            Note note;
            note.lyric = QStringLiteral("a");
            note.noteNum = 60 + i;
            note.length = 480;
            track.notes.push_back(note);
        }
        Project project;
        project.tracks.push_back(track);
        SynthPlan::Options options;
        options.cacheDirectory = root() / "cache";
        options.outputFile = root() / "out.wav";
        return SynthPlan::make(project, *bank, options, diagnostics);
    }

    /// A script that writes the fragments of \a written, in this order, and then ends as
    /// \a ending specifies.
    class ScriptStandIn : public SynthToolProcess {
    public:
        ScriptStandIn(QList<std::filesystem::path> written, SynthToolRun ending)
            : m_written(std::move(written)), m_ending(std::move(ending)) {
        }

        SynthToolRun runScript(const std::filesystem::path &, DiagnosticList &,
                               const std::function<bool()> &) const override {
            for (const auto &path : m_written) {
                std::ofstream(path, std::ios::binary | std::ios::trunc) << "RIFF piece";
            }
            return m_ending;
        }

    private:
        QList<std::filesystem::path> m_written;
        SynthToolRun m_ending;
    };

    class ScriptRunner : public ClassicSynthRunner {
    public:
        QList<std::filesystem::path> written;
        SynthToolRun ending;

        std::unique_ptr<SynthToolProcess> makeSynthToolProcess() const override {
            return std::make_unique<ScriptStandIn>(written, ending);
        }
    };

    static SynthToolRun killedBy(bool cancelled) {
        SynthToolRun run;
        run.started = true;
        run.cancelled = cancelled;
        run.timedOut = !cancelled;
        return run;
    }

    /// Writes every fragment of \a plan, as an earlier render leaves them, and waits until a
    /// later write has a different time.
    static void fillCache(const SynthPlan &plan) {
        std::filesystem::create_directories(plan.cacheDirectory());
        for (const auto &step : plan.steps()) {
            std::ofstream(step.cacheFile, std::ios::binary) << "RIFF earlier";
        }
        QThread::msleep(50);
    }

    static QList<bool> existing(const SynthPlan &plan) {
        QList<bool> result;
        for (const auto &step : plan.steps()) {
            result.push_back(std::filesystem::exists(step.cacheFile));
        }
        return result;
    }

    /// Returns the first of \a candidates that the system code page represents if
    /// \a representable, or cannot represent otherwise, or an empty string if none.
    static QString systemCandidate(bool representable, const QStringList &candidates) {
        const TextCodec codec;
        for (const auto &candidate : candidates) {
            if (codec.canEncode(candidate) == representable) {
                return candidate;
            }
        }
        return {};
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void cleanup() {
        m_dir.reset();
    }

    // The layout expected by a synth tool that reads the script.
    void the_script_has_the_shape_utau_writes() {
        const auto plan = planFor(QStringLiteral("a"), QString());
        QVERIFY(plan.has_value());

        ClassicSynthRunner runner;
        runner.shell = ClassicSynthRunner::ScriptShell::Batch;
        DiagnosticList diagnostics;
        const auto written = runner.scripts(*plan, synthTools(), diagnostics);
        QVERIFY(written.has_value());

        const auto &bat = written->first;
        QVERIFY(bat.contains(QLatin1String("@set \"tool=")));
        QVERIFY(bat.contains(QLatin1String("@set \"resamp=")));
        QVERIFY(bat.contains(QLatin1String("@set \"output=")));
        QVERIFY(bat.contains(QLatin1String("@set \"cachedir=")));
        QVERIFY(bat.contains(QLatin1String("@call \"%helper%\"")));
        QVERIFY(bat.contains(QLatin1String("@mkdir \"%cachedir%\" 2>nul")));
        QVERIFY(bat.contains(QLatin1String("@echo ") + QString(40, QLatin1Char('#')) +
                             QLatin1String("(1/1)")));

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
        runner.shell = ClassicSynthRunner::ScriptShell::Batch;
        DiagnosticList diagnostics;
        const auto written = runner.scripts(*plan, synthTools(), diagnostics);
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

    // The fragment is assigned in the form UTAU writes, which moresampler reads from the script,
    // and still no operator in it reaches cmd: read as cmd reads it, the line sets exactly the
    // fragment. The lyric is part of the fragment name.
    void the_fragment_is_set_as_utau_writes_it_data() {
        QTest::addColumn<QString>("lyric");

        QTest::newRow("plain") << QStringLiteral("a");
        QTest::newRow("ampersand") << QStringLiteral("a&whoami");
        QTest::newRow("parenthesis") << QStringLiteral("a)&whoami&(");
        QTest::newRow("caret") << QStringLiteral("a^&whoami");
        QTest::newRow("variable") << QStringLiteral("%PATH%");
    }

    void the_fragment_is_set_as_utau_writes_it() {
        QFETCH(QString, lyric);

        const auto plan = planFor(lyric, QString());
        QVERIFY(plan.has_value());

        ClassicSynthRunner runner;
        runner.shell = ClassicSynthRunner::ScriptShell::Batch;
        DiagnosticList diagnostics;
        const auto written = runner.scripts(*plan, synthTools(), diagnostics);
        QVERIFY(written.has_value());

        QString line;
        for (const auto &l : written->first.split(QLatin1String("\r\n"))) {
            if (l.startsWith(QLatin1String("@set temp="))) {
                line = l;
            }
        }
        QVERIFY2(!line.isEmpty(), qPrintable(written->first));

        // A caret makes the next character literal, a pair of percent signs is one, and an
        // operator without a caret would end the assignment.
        const auto raw = line.mid(QLatin1String("@set temp=").size());
        QString value;
        for (qsizetype i = 0; i < raw.size(); ++i) {
            const QChar c = raw.at(i);
            if (c == QLatin1Char('^')) {
                QVERIFY(i + 1 < raw.size());
                value += raw.at(++i);
            } else if (c == QLatin1Char('%')) {
                QVERIFY2(i + 1 < raw.size() && raw.at(i + 1) == QLatin1Char('%'), qPrintable(raw));
                value += c;
                ++i;
            } else {
                QVERIFY2(!QLatin1String("&|<>()\"").contains(c), qPrintable(raw));
                value += c;
            }
        }
        QCOMPARE(value, plan->steps().first().resamplerArguments.at(1));
    }

    // The mode for a synth tool that requires the script text to match UTAU exactly. It is unsafe,
    // and the test asserts this explicitly.
    void the_verbatim_mode_writes_what_it_was_given() {
        const auto plan = planFor(QStringLiteral("a"), QStringLiteral("g-5&whoami"));
        QVERIFY(plan.has_value());

        ClassicSynthRunner runner;
        runner.shell = ClassicSynthRunner::ScriptShell::Batch;
        runner.quoting = ClassicSynthRunner::Quoting::Verbatim;

        DiagnosticList diagnostics;
        const auto written = runner.scripts(*plan, synthTools(), diagnostics);
        QVERIFY(written.has_value());

        QVERIFY(written->first.contains(QLatin1String("@set flag=g-5&whoami")));
    }

    // Neither character can be escaped inside set "name=value", so the value is rejected rather
    // than written in corrupted form.
    void a_value_a_script_cannot_carry_is_refused() {
        const auto plan = planFor(QStringLiteral("a"), QStringLiteral("g-5\nwhoami"));
        QVERIFY(plan.has_value());

        ClassicSynthRunner runner;
        runner.shell = ClassicSynthRunner::ScriptShell::Batch;
        DiagnosticList diagnostics;
        QVERIFY(!runner.scripts(*plan, synthTools(), diagnostics).has_value());
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
        runner.shell = ClassicSynthRunner::ScriptShell::Batch;
        const auto written = runner.scripts(*plan, synthTools(), diagnostics);
        QVERIFY(written.has_value());

        QVERIFY(written->first.contains(QLatin1String("@\"%tool%\" ")));
        QVERIFY(!written->first.contains(QLatin1String("@call")));
    }

    // Every path passed to a synth tool must tolerate a space in a folder name. The default voice
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
        runner.shell = ClassicSynthRunner::ScriptShell::Batch;
        const auto written = runner.scripts(*plan, synthTools(), diagnostics);
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

    // A percent sign in a path would start a variable in a batch file, and a single quote would
    // end the quoting in a shell script. Read as each shell reads it, the first argument of the
    // helper is the sample.
    void a_path_with_a_quote_or_a_percent_sign_arrives_as_it_is_data() {
        QTest::addColumn<bool>("batch");

        QTest::newRow("batch") << true;
        QTest::newRow("shell") << false;
    }

    void a_path_with_a_quote_or_a_percent_sign_arrives_as_it_is() {
        QFETCH(bool, batch);

        write(QStringLiteral("it's_100%/oto.ini"), "a.wav=a,10,20,30,40,5\n");
        write(QStringLiteral("it's_100%/a.wav"), "RIFF");
        FixedCharsetSelector selector(QStringLiteral("UTF-8"));
        DiagnosticList diagnostics;
        const auto bank = VoiceBank::open(root() / "it's_100%", &selector, diagnostics);
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

        ClassicSynthRunner runner;
        runner.shell =
            batch ? ClassicSynthRunner::ScriptShell::Batch : ClassicSynthRunner::ScriptShell::Posix;
        const auto written = runner.scripts(*plan, synthTools(), diagnostics);
        QVERIFY(written.has_value());

        const QString call =
            batch ? QStringLiteral("@call \"%helper%\" ") : QStringLiteral("\"${helper}\" ");
        QString line;
        for (const auto &l :
             written->first.split(batch ? QStringLiteral("\r\n") : QStringLiteral("\n"))) {
            if (l.startsWith(call)) {
                line = l.mid(call.size());
            }
        }
        QVERIFY2(!line.isEmpty(), qPrintable(written->first));

        // The first argument, as the shell reads it
        QString value;
        bool quoted = false;
        for (qsizetype i = 0; i < line.size(); ++i) {
            const QChar c = line.at(i);
            if (batch) {
                if (c == QLatin1Char('%')) {
                    QVERIFY2(i + 1 < line.size() && line.at(i + 1) == QLatin1Char('%'),
                             qPrintable(line));
                    value += c;
                    ++i;
                    continue;
                }
                if (c == QLatin1Char('"')) {
                    quoted = !quoted;
                    continue;
                }
            } else {
                if (c == QLatin1Char('\'')) {
                    quoted = !quoted;
                    continue;
                }
                if (!quoted && c == QLatin1Char('\\')) {
                    value += line.at(++i);
                    continue;
                }
            }
            if (!quoted && c == QLatin1Char(' ')) {
                break;
            }
            value += c;
        }
        QCOMPARE(value, plan->steps().first().resamplerArguments.at(0));
    }

    // The same for a value without any other character that requires quotes, which a path on
    // Windows always has in its separators.
    void a_lone_quote_or_percent_sign_is_escaped() {
        const ShellSyntax shell(ClassicSynthRunner::ScriptShell::Posix,
                                ClassicSynthRunner::Quoting::Escaped);
        QCOMPARE(shell.argument(QStringLiteral("it's")), QStringLiteral("'it'\\''s'"));
        QCOMPARE(shell.argument(QStringLiteral("100%")), QStringLiteral("100%"));

        const ShellSyntax batch(ClassicSynthRunner::ScriptShell::Batch,
                                ClassicSynthRunner::Quoting::Escaped);
        QCOMPARE(batch.argument(QStringLiteral("100%")), QStringLiteral("100%%"));
        QCOMPARE(batch.argument(QStringLiteral("it's")), QStringLiteral("it's"));
    }

    // On other systems the UTAU synth tools run under Wine, and the script that starts them is a
    // shell script. The layout is the same and the syntax differs. The script is generated and
    // verified here, so that its content is not first observed when a user runs it.
    void the_shell_script_has_the_same_shape() {
        const auto plan = planFor(QStringLiteral("a"), QString());
        QVERIFY(plan.has_value());

        ClassicSynthRunner runner;
        runner.shell = ClassicSynthRunner::ScriptShell::Posix;

        DiagnosticList diagnostics;
        const auto written = runner.scripts(*plan, synthTools(), diagnostics);
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
        const auto written = runner.scripts(*plan, synthTools(), diagnostics);
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

        // Nonexistent synth tools still produce a script, because writing a script executes
        // nothing.
        ClassicSynthRunner runner;
        runner.shell = ClassicSynthRunner::ScriptShell::Batch;
        diagnostics.clear();
        QVERIFY(runner.scripts(*plan, synthTools(), diagnostics).has_value());
        QVERIFY(diagnostics.isEmpty());
    }

    // The progress bar as UTAU writes it, halves rounded to even
    void the_progress_is_shown_as_utau_shows_it() {
        const auto bar = [](int filled) {
            return QString(filled, QLatin1Char('#')) + QString(40 - filled, QLatin1Char('-'));
        };
        QCOMPARE(ShellSyntax::progress(1, 16), bar(2) + QLatin1String("(1/16)"));
        QCOMPARE(ShellSyntax::progress(5, 16), bar(12) + QLatin1String("(5/16)"));
        QCOMPARE(ShellSyntax::progress(3, 16), bar(8) + QLatin1String("(3/16)"));
        QCOMPARE(ShellSyntax::progress(16, 51), bar(13) + QLatin1String("(16/51)"));
        QCOMPARE(ShellSyntax::progress(50, 51), bar(39) + QLatin1String("(50/51)"));
    }

    // A script that writes nothing fails, though the track file of an earlier render is there,
    // and the script of a track is allowed the time of each of its synth tool calls.
    void a_script_is_judged_by_what_it_writes_in_its_time() {
        const auto plan = planFor(QStringLiteral("a"), QString());
        QVERIFY(plan.has_value());
        write(QStringLiteral("out.wav"), "RIFF");

        StubbedRunner runner;
        runner.scriptDirectory = root() / "script";
        DiagnosticList diagnostics;
        const auto outcome = runner.render(*plan, synthTools(), nullptr, diagnostics);
        QVERIFY(!outcome.rendered);
        QVERIFY(!QFile::exists(m_dir->path() + QStringLiteral("/out.wav")));
        QCOMPARE(diagnostics.size(), 1);

        QCOMPARE(runner.timeout, SynthToolProcess().timeout * 2 * int(plan->steps().size()));
    }

    // A killed script may have killed a resampler in the middle of its fragment. The notes run
    // in track order, so the fragment in doubt is the last one that the script created or
    // rewrote, which is removed, whether the script was cancelled or timed out.
    void the_last_fragment_written_by_a_killed_script_is_removed() {
        for (const bool cancelled : {true, false}) {
            const auto p = planOfNotes(3);
            QVERIFY(p.has_value());
            std::filesystem::create_directories(p->cacheDirectory());
            ScriptRunner runner;
            runner.written = {p->steps().at(0).cacheFile, p->steps().at(1).cacheFile};
            runner.ending = killedBy(cancelled);

            DiagnosticList diagnostics;
            const auto outcome = runner.render(*p, synthTools(), nullptr, diagnostics);
            QCOMPARE(outcome.cancelled, cancelled);
            QCOMPARE(existing(*p), (QList<bool>{true, false, false}));
            for (const auto &step : p->steps()) {
                std::filesystem::remove(step.cacheFile);
            }
        }
    }

    // Fragments that existed before the script and that it did not rewrite are kept, also if
    // reuse is turned off.
    void a_fragment_the_killed_script_did_not_write_is_kept() {
        const auto p = planOfNotes(3);
        QVERIFY(p.has_value());
        fillCache(*p);
        ScriptRunner runner;
        runner.reuseCache = false;
        runner.written = {p->steps().at(0).cacheFile, p->steps().at(1).cacheFile};
        runner.ending = killedBy(true);

        DiagnosticList diagnostics;
        runner.render(*p, synthTools(), nullptr, diagnostics);
        QCOMPARE(existing(*p), (QList<bool>{true, false, true}));
    }

    void a_killed_script_that_wrote_nothing_removes_nothing() {
        const auto p = planOfNotes(3);
        QVERIFY(p.has_value());
        fillCache(*p);
        ScriptRunner runner;
        runner.ending = killedBy(false);

        DiagnosticList diagnostics;
        runner.render(*p, synthTools(), nullptr, diagnostics);
        QCOMPARE(existing(*p), (QList<bool>{true, true, true}));
    }

    // The command processor reads a batch file in the system code page. A character outside it
    // is refused, with the character in the message, rather than written as a question mark as
    // UTAU does, which leaves the note silent without a report.
    void a_character_outside_the_system_code_page_is_refused() {
        const TextCodec codec;
        if (codec.isUtf8()) {
            QSKIP("The system code page is UTF-8, which represents every character.");
        }
        const auto outside = systemCandidate(false, {QString::fromUtf8("\xF0\xA0\xAE\xB7"),
                                                     QString::fromUtf8("\xE4\xBD\xA0"),
                                                     QString::fromUtf8("\xE6\xAD\x8C")});
        QVERIFY(!outside.isEmpty());
        const auto plan = planFor(outside, QString());
        QVERIFY(plan.has_value());

        ClassicSynthRunner runner;
        DiagnosticList diagnostics;
        QVERIFY(!runner.scriptFiles(root() / "scripts", *plan, synthTools(), diagnostics));
        QVERIFY(hasError(diagnostics));
        QVERIFY2(diagnostics.last().message.contains(outside),
                 qPrintable(diagnostics.last().message));
    }

    // A character inside the system code page is written in it, through TextCodec.
    void a_script_is_written_in_the_system_code_page() {
        const TextCodec codec;
        const auto inside = systemCandidate(
            true, {QString::fromUtf8("\xE6\xAD\x8C"), QString::fromUtf8("\xC3\xA9")});
        if (inside.isEmpty()) {
            QSKIP("The system code page represents no candidate.");
        }
        const auto plan = planFor(inside, QString());
        QVERIFY(plan.has_value());

        ClassicSynthRunner runner;
        DiagnosticList diagnostics;
        const auto files = runner.scriptFiles(root() / "scripts", *plan, synthTools(), diagnostics);
        QVERIFY(files.has_value());
        const auto text = codec.decode(files->first().second);
        QVERIFY(text.has_value());
        QVERIFY(text->contains(inside));
    }

    // A quote in the flags is removed by stdutau, so that it does not refuse the script.
    void a_quote_in_the_flags_does_not_refuse_the_script() {
        const auto plan = planFor(QStringLiteral("a"), QStringLiteral("g\"5"));
        QVERIFY(plan.has_value());
        ClassicSynthRunner runner;
        DiagnosticList diagnostics;
        QVERIFY(runner.scriptFiles(root() / "scripts", *plan, synthTools(), diagnostics));
    }

    // A note with $direct calls the wavtool with its sample, without the helper that runs the
    // resampler, as UTAU writes it.
    void a_direct_note_calls_the_wavtool_with_its_sample() {
        auto plan = planFor(QStringLiteral("a"), QString());
        QVERIFY(plan.has_value());
        Note note;
        note.lyric = QStringLiteral("a");
        note.noteNum = 60;
        note.length = 480;
        note.direct = QStringLiteral("True");
        Project project;
        project.tracks.push_back({});
        project.tracks[0].notes.push_back(note);
        FixedCharsetSelector selector(QStringLiteral("UTF-8"));
        DiagnosticList diagnostics;
        const auto bank = VoiceBank::open(root() / "bank", &selector, diagnostics);
        QVERIFY(bank.has_value());
        SynthPlan::Options options;
        options.cacheDirectory = root() / "cache";
        options.outputFile = root() / "out.wav";
        plan = SynthPlan::make(project, *bank, options, diagnostics);
        QVERIFY(plan.has_value());
        QVERIFY(plan->steps().at(0).direct);

        ClassicSynthRunner runner;
        runner.shell = ClassicSynthRunner::ScriptShell::Batch;
        const auto written = runner.scripts(*plan, synthTools(), diagnostics);
        QVERIFY(written.has_value());
        QVERIFY(written->first.contains(QLatin1String("@\"%tool%\" ")));
        QVERIFY(!written->first.contains(QLatin1String("@call")));
        QVERIFY(written->first.contains(QLatin1String("a.wav")));
    }
};

QTEST_APPLESS_MAIN(test_ClassicSynthRunner)

#include "test_ClassicSynthRunner.moc"
