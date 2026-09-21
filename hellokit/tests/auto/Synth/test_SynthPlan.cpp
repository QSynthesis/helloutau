#include <functional>
#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Synth/SynthPlan.h>

using namespace hello::kit;

class test_SynthPlan : public QObject {
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

    /// A bank that sings \c a , \c ka and \c ki .
    std::optional<VoiceBank> bank() {
        write(QStringLiteral("bank/oto.ini"), "a.wav=a,10,20,30,40,5\n"
                                              "ka.wav=ka,11,21,31,41,6\n"
                                              "ki.wav=ki,12,22,32,42,7\n");
        write(QStringLiteral("bank/a.wav"), "RIFF");
        write(QStringLiteral("bank/ka.wav"), "RIFF");
        write(QStringLiteral("bank/ki.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("UTF-8"));
        DiagnosticList diagnostics;
        return VoiceBank::open(root() / "bank", &selector, diagnostics);
    }

    static Note note(const QString &lyric, int noteNum = 60, int length = 480) {
        Note n;
        n.lyric = lyric;
        n.noteNum = noteNum;
        n.length = length;
        return n;
    }

    static Project projectOf(const QList<Note> &notes, double tempo = 120.0) {
        Project project;
        project.settings.tempo = tempo;
        Track track;
        track.notes = notes;
        project.tracks.push_back(track);
        return project;
    }

    SynthPlan::Options options() const {
        SynthPlan::Options o;
        o.cacheDirectory = root() / "cache";
        o.outputFile = root() / "out.wav";
        return o;
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void cleanup() {
        m_dir.reset();
    }

    void it_makes_one_step_per_note() {
        const auto voices = bank();
        QVERIFY(voices.has_value());

        DiagnosticList diagnostics;
        const auto plan =
            SynthPlan::make(projectOf({note(QStringLiteral("a")), note(QStringLiteral("ka")),
                                       note(QStringLiteral("ki"))}),
                            *voices, options(), diagnostics);

        QVERIFY(plan.has_value());
        QCOMPARE(plan->steps().size(), 3);
        for (int i = 0; i < 3; ++i) {
            QCOMPARE(plan->steps().at(i).noteIndex, i);
        }
    }

    // The seam this class exists for: a lyric becomes a sample through the voice bank, and the
    // path that reaches the engine is the whole path, not the name the oto.ini carried.
    void a_lyric_becomes_the_sample_the_bank_resolves_it_to() {
        const auto voices = bank();
        QVERIFY(voices.has_value());

        DiagnosticList diagnostics;
        const auto plan = SynthPlan::make(projectOf({note(QStringLiteral("ka"))}), *voices,
                                          options(), diagnostics);
        QVERIFY(plan.has_value());

        const auto &step = plan->steps().at(0);
        QVERIFY(!step.silent);
        QCOMPARE(step.sample, root() / "bank" / "ka.wav");

        // The path that reaches the engine is that sample, spelled the way the bank settled it.
        QCOMPARE(step.resamplerArguments.at(0), QString::fromStdU16String(step.sample.u16string()));
    }

    // The cut belongs to the sample, and it has to arrive at the engine in the order the engine
    // reads its arguments in. Getting one of these wrong renders, and renders wrongly.
    void the_oto_entry_reaches_the_resampler() {
        const auto voices = bank();
        QVERIFY(voices.has_value());

        DiagnosticList diagnostics;
        const auto plan = SynthPlan::make(projectOf({note(QStringLiteral("ka"))}), *voices,
                                          options(), diagnostics);
        QVERIFY(plan.has_value());

        const auto &arguments = plan->steps().at(0).resamplerArguments;
        QCOMPARE(arguments.at(2), QStringLiteral("C4")); // tone name for note 60
        QCOMPARE(arguments.at(5), QStringLiteral("11")); // offset
        QCOMPARE(arguments.at(7), QStringLiteral("21")); // consonant
        QCOMPARE(arguments.at(8), QStringLiteral("31")); // cutoff
    }

    // A piece already on disk is taken as done and the resampler is not run again, so anything
    // that changes what the resampler would produce has to change where it would put it.
    // Otherwise the second render hands back the first one's sound.
    void a_note_rendered_differently_caches_somewhere_else() {
        const auto voices = bank();
        QVERIFY(voices.has_value());

        const auto nameFor = [&](const std::function<void(Note &)> &change) {
            auto n = note(QStringLiteral("a"));
            change(n);
            DiagnosticList diagnostics;
            const auto plan = SynthPlan::make(projectOf({n}), *voices, options(), diagnostics);
            return plan ? QString::fromStdU16String(plan->steps().at(0).cacheFile.u16string())
                        : QString();
        };

        const QString plain = nameFor([](Note &) {});
        QVERIFY(!plain.isEmpty());
        QCOMPARE(nameFor([](Note &) {}), plain); // the same note twice is the same piece

        // Everything the resampler is handed.
        QVERIFY(nameFor([](Note &n) { n.flags = QStringLiteral("g5"); }) != plain);
        QVERIFY(nameFor([](Note &n) { n.intensity = 80; }) != plain);
        QVERIFY(nameFor([](Note &n) { n.modulation = 50; }) != plain);
        QVERIFY(nameFor([](Note &n) { n.velocity = 150; }) != plain);
        QVERIFY(nameFor([](Note &n) { n.noteNum = 62; }) != plain);
        QVERIFY(nameFor([](Note &n) { n.length = 240; }) != plain);
        QVERIFY(nameFor([](Note &n) {
                    Vibrato vibrato;
                    vibrato.length = 80;
                    vibrato.period = 180;
                    vibrato.amplitude = 100;
                    n.vibrato = vibrato;
                }) != plain);
    }

    // And the other way: the envelope is applied while the piece is appended to the track, not
    // while it is rendered, so two notes differing only there are the same piece and share it.
    void what_the_wavtool_does_is_not_part_of_the_piece() {
        const auto voices = bank();
        QVERIFY(voices.has_value());

        DiagnosticList diagnostics;
        const auto plain = SynthPlan::make(projectOf({note(QStringLiteral("a"))}), *voices,
                                           options(), diagnostics);
        QVERIFY(plain.has_value());

        auto other = note(QStringLiteral("a"));
        Envelope envelope;
        envelope.anchors = {
            {0,  0  },
            {12, 100},
            {42, 100},
            {0,  0  }
        };
        other.envelope = envelope;
        const auto changed = SynthPlan::make(projectOf({other}), *voices, options(), diagnostics);
        QVERIFY(changed.has_value());

        // If this one ever fails, the envelope has started reaching the resampler and the cache
        // name has to follow it after all.
        QCOMPARE(changed->steps().at(0).resamplerArguments,
                 plain->steps().at(0).resamplerArguments);
        QVERIFY(changed->steps().at(0).wavtoolArguments != plain->steps().at(0).wavtoolArguments);
        QCOMPARE(changed->steps().at(0).cacheFile, plain->steps().at(0).cacheFile);
    }

    void the_cache_file_goes_where_it_was_asked_to() {
        const auto voices = bank();
        QVERIFY(voices.has_value());

        DiagnosticList diagnostics;
        const auto plan = SynthPlan::make(projectOf({note(QStringLiteral("a"))}), *voices,
                                          options(), diagnostics);
        QVERIFY(plan.has_value());

        const auto &step = plan->steps().at(0);
        QCOMPARE(step.cacheFile.parent_path(), root() / "cache");
        QCOMPARE(step.resamplerArguments.at(1),
                 QString::fromStdU16String(step.cacheFile.u16string()));

        // The wavtool appends that same piece to the track, and the track comes first: the
        // engine reads its arguments as <outfile> <infile>.
        const auto &wavtool = step.wavtoolArguments;
        QCOMPARE(wavtool.at(0), QString::fromStdU16String((root() / "out.wav").u16string()));
        QCOMPARE(wavtool.at(1), QString::fromStdU16String(step.cacheFile.u16string()));
    }

    // A rest has no sample, so there is nothing to resample. The wavtool still runs, because a
    // rest is how silence gets its length in the track.
    void a_rest_gets_no_resampler_call() {
        const auto voices = bank();
        QVERIFY(voices.has_value());

        DiagnosticList diagnostics;
        const auto plan =
            SynthPlan::make(projectOf({note(QStringLiteral("a")), note(QStringLiteral("R")),
                                       note(QStringLiteral("ka"))}),
                            *voices, options(), diagnostics);
        QVERIFY(plan.has_value());

        const auto &rest = plan->steps().at(1);
        QVERIFY(rest.silent);
        QVERIFY(rest.resamplerArguments.isEmpty());
        QVERIFY(!rest.wavtoolArguments.isEmpty());

        // Nothing is wrong with a rest, so nothing is said about it.
        QVERIFY(diagnostics.isEmpty());
    }

    // A lyric the bank cannot sing is silent too, but that one the user wants to hear about.
    void a_lyric_the_bank_cannot_sing_is_reported() {
        const auto voices = bank();
        QVERIFY(voices.has_value());

        DiagnosticList diagnostics;
        const auto plan = SynthPlan::make(projectOf({note(QStringLiteral("zzz"))}), *voices,
                                          options(), diagnostics);
        QVERIFY(plan.has_value());

        QVERIFY(plan->steps().at(0).silent);
        QCOMPARE(diagnostics.size(), 1);
        QCOMPARE(diagnostics.at(0).severity, DiagnosticSeverity::Warning);
        QCOMPARE(diagnostics.at(0).noteIndex, 0);
    }

    // The note's own flags come first and the project's are appended. UTAU's own temp.bat for a
    // 455-note probe says so on 141 of the 164 notes that carried flags of their own.
    void the_flags_of_the_project_and_of_the_note_both_arrive() {
        const auto voices = bank();
        QVERIFY(voices.has_value());

        auto n = note(QStringLiteral("a"));
        n.flags = QStringLiteral("g-5");
        auto project = projectOf({n});
        project.settings.flags = QStringLiteral("B50");

        DiagnosticList diagnostics;
        const auto plan = SynthPlan::make(project, *voices, options(), diagnostics);
        QVERIFY(plan.has_value());

        QCOMPARE(plan->steps().at(0).resamplerArguments.at(4), QStringLiteral("g-5B50"));
    }

    // Rendering a selection is not the same as rendering it as though nothing else were there:
    // pre-utterance and overlap are settled between neighbours, so the notes outside the range
    // still have to be read.
    void a_range_renders_only_its_own_notes() {
        const auto voices = bank();
        QVERIFY(voices.has_value());

        auto o = options();
        o.range = std::make_pair(1, 2);

        DiagnosticList diagnostics;
        const auto plan =
            SynthPlan::make(projectOf({note(QStringLiteral("a")), note(QStringLiteral("ka")),
                                       note(QStringLiteral("ki")), note(QStringLiteral("a"))}),
                            *voices, o, diagnostics);
        QVERIFY(plan.has_value());

        QCOMPARE(plan->steps().size(), 2);
        QCOMPARE(plan->steps().at(0).noteIndex, 1);
        QCOMPARE(plan->steps().at(1).noteIndex, 2);
    }

    void a_range_outside_the_track_is_refused() {
        const auto voices = bank();
        QVERIFY(voices.has_value());

        auto o = options();
        o.range = std::make_pair(0, 5);

        DiagnosticList diagnostics;
        QVERIFY(!SynthPlan::make(projectOf({note(QStringLiteral("a"))}), *voices, o, diagnostics)
                     .has_value());
        QVERIFY(hasError(diagnostics));
    }

    void a_project_with_nothing_in_it_is_refused() {
        const auto voices = bank();
        QVERIFY(voices.has_value());

        DiagnosticList diagnostics;
        QVERIFY(!SynthPlan::make(projectOf({}), *voices, options(), diagnostics).has_value());
        QVERIFY(hasError(diagnostics));
    }

    void a_render_with_nowhere_to_write_is_refused() {
        const auto voices = bank();
        QVERIFY(voices.has_value());

        SynthPlan::Options o;
        o.cacheDirectory = root() / "cache";

        DiagnosticList diagnostics;
        QVERIFY(!SynthPlan::make(projectOf({note(QStringLiteral("a"))}), *voices, o, diagnostics)
                     .has_value());
        QVERIFY(hasError(diagnostics));
    }

    // Every argument is one argument, whatever is in it. The engines are started from a vector
    // and never from a command line, and a lyric or a flags string is not this program's to
    // vouch for.
    void nothing_in_a_note_can_split_an_argument() {
        const auto voices = bank();
        QVERIFY(voices.has_value());

        auto n = note(QStringLiteral("a"));
        n.flags = QStringLiteral("B50 & whoami");
        auto project = projectOf({n});

        DiagnosticList diagnostics;
        const auto plan = SynthPlan::make(project, *voices, options(), diagnostics);
        QVERIFY(plan.has_value());

        const auto &arguments = plan->steps().at(0).resamplerArguments;
        QVERIFY(arguments.at(4).contains(QLatin1Char('&')));
        QCOMPARE(arguments.at(4), QStringLiteral("B50 & whoami"));
    }
};

QTEST_APPLESS_MAIN(test_SynthPlan)

#include "test_SynthPlan.moc"
