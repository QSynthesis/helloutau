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

    /// A voice bank with samples for \c a , \c ka and \c ki .
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

    // The purpose of this class: a lyric resolves to a sample through the voice bank, and the
    // engine receives the full path, not the name in the oto.ini.
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

        // The engine receives the path of that sample as resolved by the voice bank.
        QCOMPARE(step.resamplerArguments.at(0), QString::fromStdU16String(step.sample.u16string()));
    }

    // The timing parameters belong to the sample and must reach the engine in the order in
    // which it reads its arguments. An error here still renders, but incorrectly.
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

    // An existing fragment is treated as complete and the resampler is not run again, so every
    // input that affects the resampler output must change the fragment name. Otherwise the
    // second render would return the sound of the first.
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
        QCOMPARE(nameFor([](Note &) {}), plain); // identical notes share a fragment

        // Every resampler argument.
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

    // Conversely, the envelope is applied when the fragment is appended to the track, not when
    // it is rendered, so two notes that differ only in the envelope share a fragment.
    void what_the_wavtool_does_is_not_part_of_the_piece() {
        const auto voices = bank();
        QVERIFY(voices.has_value());

        DiagnosticList diagnostics;
        const auto plain = SynthPlan::make(projectOf({note(QStringLiteral("a"))}), *voices,
                                           options(), diagnostics);
        QVERIFY(plain.has_value());

        auto other = note(QStringLiteral("a"));
        other.envelope = Envelope::fromTimeOrder({
            {0,  0  },
            {12, 100},
            {42, 100},
            {0,  0  }
        });
        const auto changed = SynthPlan::make(projectOf({other}), *voices, options(), diagnostics);
        QVERIFY(changed.has_value());

        // A failure here indicates that the envelope now reaches the resampler, and the cache name
        // must then include it.
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

        // The wavtool appends the same fragment to the track, with the track first, because the
        // engine reads its arguments as <outfile> <infile>.
        const auto &wavtool = step.wavtoolArguments;
        QCOMPARE(wavtool.at(0), QString::fromStdU16String((root() / "out.wav").u16string()));
        QCOMPARE(wavtool.at(1), QString::fromStdU16String(step.cacheFile.u16string()));
    }

    // A rest has no sample and requires no resampling. The wavtool still runs, because a rest
    // supplies the duration of silence in the track.
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

        // A rest is not an error, so no diagnostic is produced.
        QVERIFY(diagnostics.isEmpty());
    }

    // A lyric without a sample in the voice bank is also silent, but it must be reported to the
    // user.
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

    // The flags of the note come first and the flags of the project are appended. The temp.bat
    // UTAU wrote for a 455-note probe confirms this for 141 of the 164 notes with their own
    // flags.
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

    // Rendering a selection differs from rendering it in isolation: pre-utterance and overlap
    // are determined between neighbors, so notes outside the range must still be read.
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

    // Every argument remains a single argument regardless of its content. The engines are
    // started from a vector, never from a command line, because the content of a lyric or a
    // flags string cannot be trusted.
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
