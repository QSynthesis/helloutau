#include <memory>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QRandomGenerator>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Document/TempoMap.h>
#include <hellokit/Synth/PitchCurve.h>
#include <hellokit/Synth/SynthPlan.h>

using namespace hello::kit;

class test_PitchCurve : public QObject {
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

    // A voice bank with samples for a, ka, ki and ko, whose pre-utterances and overlaps differ,
    // so that the curves start and end at different points; that of ko ends past the start of
    // the next note, its overlap exceeding its pre-utterance.
    std::optional<VoiceBank> bank() {
        write(QStringLiteral("bank/oto.ini"), "a.wav=a,10,20,30,40,5\n"
                                              "ka.wav=ka,11,21,31,85,20\n"
                                              "ki.wav=ki,12,22,32,120,60\n"
                                              "ko.wav=ko,13,23,33,30,80\n");
        for (const auto name : {"a", "ka", "ki", "ko"}) {
            write(QStringLiteral("bank/%1.wav").arg(QLatin1String(name)), "RIFF");
        }
        FixedCharsetSelector selector(QStringLiteral("UTF-8"));
        DiagnosticList diagnostics;
        return VoiceBank::open(root() / "bank", &selector, diagnostics);
    }

    static Note note(const QString &lyric, int noteNum, int length = 480) {
        Note n;
        n.lyric = lyric;
        n.noteNum = noteNum;
        n.length = length;
        return n;
    }

    static PortamentoPoint point(double x, double cents,
                                 PortamentoPoint::Type type = PortamentoPoint::S) {
        PortamentoPoint p;
        p.x = x;
        p.y = cents;
        p.type = type;
        return p;
    }

    // Notes of every kind the curve distinguishes: rests, tempo changes, points of each type
    // before and after the start, fractional heights, and vibratos long and short, faded and
    // offset
    static QList<Note> randomNotes(quint32 seed) {
        QRandomGenerator random(seed);
        const auto between = [&random](int low, int high) { return random.bounded(low, high + 1); };
        const QStringList lyrics = {QStringLiteral("a"), QStringLiteral("ka"), QStringLiteral("ki"),
                                    QStringLiteral("ko"), QStringLiteral("R")};
        const int lengths[] = {30, 60, 120, 240, 480, 720, 960};

        QList<Note> notes;
        for (int i = 0; i < 40; ++i) {
            auto n = note(lyrics.at(between(0, 4)), between(50, 72),
                          between(0, 3) == 0 ? between(20, 1000) : lengths[between(0, 6)]);
            if (between(0, 4) == 0) {
                const double tempos[] = {90, 120, 150, 200, 75.5};
                n.tempo = tempos[between(0, 4)];
            }
            if (between(0, 4) == 0) {
                n.velocity = between(0, 200);
            }
            if (between(0, 9) != 0) {
                const int count = between(1, 5);
                double x = -between(0, 150);
                for (int j = 0; j < count; ++j) {
                    const double tenths = between(-400, 400) / (between(0, 1) ? 1.0 : 10.0);
                    n.portamento.push_back(point(x, PortamentoPoint::centsFromTenths(tenths),
                                                 PortamentoPoint::Type(between(0, 3))));
                    x += between(0, 120);
                }
            }
            if (between(0, 1) == 0) {
                Vibrato v;
                v.length = between(0, 100);
                v.period = between(30, 300);
                v.amplitude = between(0, 100);
                v.attack = between(0, 100);
                v.release = between(0, 100);
                v.phase = between(0, 100);
                v.offset = between(-100, 100);
                n.vibrato = v;
            }
            notes.push_back(n);
        }
        return notes;
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void cleanup() {
        m_dir.reset();
    }

    // The index of the first note whose values differ from those the resampler receives, or -1
    static Project projectOf(const QList<Note> &notes, double tempo) {
        Project project;
        project.settings.tempo = tempo;
        Track track;
        track.notes = notes;
        project.tracks.push_back(track);
        return project;
    }

    // The plan of \a project, one step for each note
    std::optional<SynthPlan> planOf(const VoiceBank &voices, const Project &project) {
        SynthPlan::Options options;
        options.cacheDirectory = root() / "cache";
        options.outputFile = root() / "out.wav";
        DiagnosticList diagnostics;
        return SynthPlan::make(project, voices, options, diagnostics);
    }

    // The timing of step \a i of \a steps
    static PitchCurve::Timing timingOf(const QList<SynthStep> &steps, qsizetype i) {
        PitchCurve::Timing timing;
        timing.preUtterance = steps[i].preUtterance;
        timing.startPoint = steps[i].startPoint;
        if (i + 1 < steps.size()) {
            timing.nextPreUtterance = steps[i + 1].preUtterance;
            timing.nextOverlap = steps[i + 1].voiceOverlap;
        }
        return timing;
    }

    qsizetype firstDifference(const VoiceBank &voices, const QList<Note> &notes, double tempo) {
        const auto project = projectOf(notes, tempo);
        const auto plan = planOf(voices, project);
        if (!plan) {
            return 0;
        }
        const auto &steps = plan->steps();
        const auto tempos = TempoMap::of(project);
        for (qsizetype i = 0; i < steps.size(); ++i) {
            if (PitchCurve(notes, i, tempos.tempo(int(i))).values(timingOf(steps, i)) !=
                steps[i].pitch) {
                return i;
            }
        }
        return -1;
    }

    // The purpose of the class: the curve is the curve the resampler receives.
    void the_values_are_those_of_the_resampler() {
        const auto voices = bank();
        QVERIFY(voices);
        for (quint32 seed = 1; seed <= 20; ++seed) {
            const auto difference =
                firstDifference(*voices, randomNotes(seed), seed % 2 ? 120 : 133);
            if (difference >= 0) {
                QFAIL(qPrintable(QStringLiteral("seed %1, note %2").arg(seed).arg(difference)));
            }
        }
    }

    // The Mode1 values of toMode1() give the curve of Mode2 value by value, the contributions of
    // the neighbours included, after every note is converted.
    void the_curve_of_mode2_converts_to_mode1() {
        const auto voices = bank();
        QVERIFY(voices);
        for (quint32 seed = 1; seed <= 20; ++seed) {
            const double tempo = seed % 2 ? 120 : 133;
            const auto notes = randomNotes(seed);
            const auto project = projectOf(notes, tempo);
            const auto plan = planOf(*voices, project);
            QVERIFY(plan);
            const auto &steps = plan->steps();
            const auto tempos = TempoMap::of(project);
            auto converted = notes;
            for (qsizetype i = 0; i < notes.size(); ++i) {
                if (!notes[i].isRest()) {
                    converted[i].pitchBend =
                        PitchCurve(notes, i, tempos.tempo(int(i))).toMode1(timingOf(steps, i));
                }
            }
            for (qsizetype i = 0; i < notes.size(); ++i) {
                const auto timing = timingOf(steps, i);
                const double t = tempos.tempo(int(i));
                if (!notes[i].isRest() && PitchCurve(converted, i, t).mode1Values(timing) !=
                                              PitchCurve(notes, i, t).values(timing)) {
                    QFAIL(qPrintable(QStringLiteral("seed %1, note %2").arg(seed).arg(i)));
                }
            }
        }
    }

    // The curve of a note reads the next note, its vibrato included, until the value after the
    // last point of that note, even where it reaches further.
    void the_next_note_is_read_up_to_its_last_point() {
        const auto voices = bank();
        QVERIFY(voices);
        auto next = note(QStringLiteral("ko"), 62, 120);
        next.portamento = {point(-20, 0), point(0, 0)};
        Vibrato v;
        v.length = 100;
        v.period = 30;
        v.amplitude = 80;
        next.vibrato = v;
        QCOMPARE(firstDifference(*voices, {note(QStringLiteral("a"), 60), next}, 120), -1);
    }

    // The first point starts at the previous note unless that is a rest, whatever PBS gives.
    void the_first_point_starts_at_the_previous_note() {
        auto second = note(QStringLiteral("ka"), 62);
        second.portamento = {point(-50, -300), point(0, 0)};

        const QList<Note> afterNote = {note(QStringLiteral("a"), 60), second};
        QCOMPARE(PitchCurve(afterNote, 1, 120).portamentoAt(-1000), -200.0);

        const QList<Note> afterRest = {note(QStringLiteral("R"), 60), second};
        QCOMPARE(PitchCurve(afterRest, 1, 120).portamentoAt(-1000), -300.0);
    }

    // Without points a note keeps the pitch of the previous note until its start.
    void a_note_without_points_bends_at_its_start() {
        const QList<Note> notes = {note(QStringLiteral("a"), 60), note(QStringLiteral("ka"), 62)};
        const PitchCurve curve(notes, 1, 120);
        QCOMPARE(curve.portamentoAt(-10), -200.0);
        QCOMPARE(curve.portamentoAt(10), 0.0);
    }

    // The previous note continues before the start, the next note from its first point, each
    // counted relative to this note.
    void the_neighbours_continue_across_the_boundary() {
        auto first = note(QStringLiteral("a"), 60);
        first.portamento = {point(0, 0), point(100, 100, PortamentoPoint::Linear),
                            point(1000, 100)};
        auto second = note(QStringLiteral("ka"), 62);
        second.portamento = {point(-100, 0), point(0, 0)};
        const QList<Note> notes = {first, second};

        // Before the first point of the second note, 100 ms or 96 ticks at 120 bpm before it,
        // the first note is at its plateau of 100 cents, which is -100 relative to the second.
        QCOMPARE(PitchCurve(notes, 1, 120).portamentoAt(-100), -100.0);
        // Seen from the first note, the second note has begun to bend towards its own pitch.
        const PitchCurve curve(notes, 0, 120);
        QVERIFY(curve.portamentoAt(480 - 48) > 100);
    }

    // The own curve of a note leaves out the neighbours and spans the points of the note that
    // lie before its start or after its end, 576 and 672 ticks at 120 bpm.
    void the_own_curve_leaves_out_the_neighbours() {
        auto first = note(QStringLiteral("a"), 60);
        Vibrato v;
        v.length = 100;
        v.period = 100;
        v.amplitude = 50;
        first.vibrato = v;
        auto second = note(QStringLiteral("ka"), 62);
        second.portamento = {point(-600, 0), point(0, 0), point(700, 0)};
        const QList<Note> notes = {first, second};

        // The points of the second note lie before the start of the first.
        const PitchCurve curve(notes, 0, 120);
        QVERIFY(curve.portamentoAt(100) > 0);
        QCOMPARE(curve.ownPortamentoAt(100), 0.0);
        QCOMPARE(curve.ownSpan(), (std::pair{0.0, 480.0}));

        const PitchCurve next(notes, 1, 120);
        QCOMPARE(next.ownSpan(), (std::pair{-576.0, 672.0}));
        QCOMPARE(next.pointSpan(), (std::pair{-576.0, 672.0}));
        second.portamento = {point(100, 0), point(200, 0)};
        QCOMPARE(PitchCurve({first, second}, 1, 120).pointSpan(), (std::pair{96.0, 192.0}));
        QCOMPARE(PitchCurve({first, second}, 1, 120).ownSpan(), (std::pair{0.0, 480.0}));
        QCOMPARE(next.ownPortamentoAt(-1000), -200.0);
        QVERIFY(next.vibratoAt(-100) != 0);
        QCOMPARE(next.ownVibratoAt(-100), 0.0);
        QVERIFY(curve.ownVibratoAt(380) != 0);
    }

    // A note of 480 ticks with Mode1 values; the probe of docs/Synth.md used such notes.
    static Note bent(const QList<double> &values, double start) {
        auto n = note(QStringLiteral("a"), 60);
        PitchBend bend;
        bend.start = start;
        bend.values = values;
        n.pitchBend = bend;
        return n;
    }

    static QList<double> ramp() {
        QList<double> values;
        for (int i = 0; i < 96; ++i) {
            values.push_back(i * 2);
        }
        return values;
    }

    // The first readings of the curve of Mode1, alone between rests at 120 bpm with the first
    // reading 8.925 ms before the note, as UTAU passed them in the probe of docs/Synth.md
    static QList<int> mode1Readings(const Note &n,
                                    const Note &before = note(QStringLiteral("R"), 60)) {
        PitchCurve::Timing timing;
        timing.preUtterance = 8.925;
        return PitchCurve({before, n}, 1, 120).mode1Values(timing);
    }

    void the_curve_of_mode1_is_that_of_utau() {
        QCOMPARE(mode1Readings(bent(ramp(), 0)).mid(0, 7), (QList<int>{0, 0, 1, 3, 5, 7, 9}));
        QCOMPARE(mode1Readings(bent(ramp(), -50)).mid(0, 4), (QList<int>{16, 18, 20, 22}));
        QCOMPARE(mode1Readings(bent(ramp(), -20.5)).mid(0, 4), (QList<int>{4, 6, 8, 10}));
        QCOMPARE(mode1Readings(bent(ramp(), 20)).mid(0, 9),
                 (QList<int>{0, 0, 0, 0, 0, 0, 1, 3, 5}));
        QCOMPARE(mode1Readings(bent(ramp(), 0)).size(), 99);

        // The interval after the last value holds it, and then it is 0.
        const auto ten = mode1Readings(bent(QList<double>(10, 100), 0));
        QCOMPARE(ten.mid(0, 13),
                 (QList<int>{0, 0, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 0}));
        QCOMPARE(ten.last(), 0);

        // Rounded, and interpolated between values
        QCOMPARE(mode1Readings(bent(QList<double>(96, 10.5), 0)).mid(0, 4),
                 (QList<int>{0, 0, 11, 11}));
        QList<double> alternating;
        for (int i = 0; i < 96; ++i) {
            alternating.push_back(i % 2 ? -300 : 300);
        }
        QCOMPARE(mode1Readings(bent(alternating, 0)).mid(0, 5), (QList<int>{0, 0, 128, -128, 128}));
    }

    // Neither the Mode2 points nor the vibrato take part in the curve of Mode1.
    void mode1_leaves_out_the_points_and_the_vibrato() {
        auto n = bent(QList<double>(96, 50), 0);
        n.portamento = {point(-40, 0), point(0, 300)};
        Vibrato v;
        v.length = 65;
        v.period = 180;
        v.amplitude = 35;
        n.vibrato = v;
        const auto readings = mode1Readings(n);
        QCOMPARE(readings.mid(0, 4), (QList<int>{0, 0, 50, 50}));
        QCOMPARE(readings.at(60), 50);
    }

    // Before the start of a note, and before its own values, the curve is that of the previous
    // note, as for Mode2. UTAU passed 188 for the first reading, which this gives as 189 (the
    // boundary of docs/Synth.md).
    void mode1_continues_the_previous_note_before_the_start() {
        const auto readings = mode1Readings(note(QStringLiteral("a"), 60), bent(ramp(), 0));
        QVERIFY(qAbs(readings.at(0) - 188) <= 1);
        QCOMPARE(readings.mid(1, 3), (QList<int>{190, 0, 0}));
    }

    // The previous note counts relative to this one: its values move by the difference of the
    // pitches, as far as they reach with the interval after them, and beyond that the curve is
    // 0. UTAU passed these readings for li after la in the tuning comparison of step 5 in
    // docs/Tuning.md, the first 85 ticks before li.
    void mode1_counts_the_previous_note_from_this_one() {
        QList<double> laValues;
        for (int k = 0; k < 26; ++k) {
            laValues.push_back(-100 + 4 * k);
        }
        laValues.append(QList<double>(60, 0));
        auto la = note(QStringLiteral("la"), 60);
        la.pitchBend = PitchBend{-31.25, laValues};
        auto li = note(QStringLiteral("li"), 62);
        li.pitchBend = PitchBend{-41.667, QList<double>(20, 50)};

        PitchCurve::Timing timing;
        timing.preUtterance = 85 * 125.0 / 120;
        QCOMPARE(PitchCurve({la, li}, 1, 120).mode1Values(timing).mid(0, 10),
                 (QList<int>{-200, 0, 0, 0, 0, 0, 0, 0, 0, 50}));

        // After a rest, as for the first Mode2 point, the values do not move.
        la.lyric = QStringLiteral("R");
        la.pitchBend->values.last() = 30;
        QCOMPARE(PitchCurve({la, li}, 1, 120).mode1Values(timing).first(), 30);
    }

    // If the values of a note end before its start, the curve between their end and the start
    // equals that of the previous note as far as its values reach, and after the start it is 0,
    // as probe 4 measured in UTAU (docs/Synth.md).
    void mode1_takes_the_previous_note_where_the_values_end_before_the_start() {
        auto a = bent(QList<double>(96, 100), 0);
        auto b = bent({50, 50}, -60 * 125.0 / 120);
        const PitchCurve curve({a, b}, 1, 120);
        QCOMPARE(curve.mode1At(-55), 50.0);
        QCOMPARE(curve.mode1At(-30), 100.0);
        QCOMPARE(curve.mode1At(10), 0.0);
    }

    // With Mode2 off the resampler receives the curve of Mode1, with it on the curve of the
    // points and the vibrato.
    void the_setting_of_the_project_chooses_the_curve() {
        const auto voices = bank();
        QVERIFY(voices);
        auto n = bent(ramp(), -20);
        n.portamento = {point(-40, 0), point(0, 300)};
        Project project;
        project.settings.tempo = 120;
        Track track;
        track.notes = {note(QStringLiteral("R"), 60), n};
        project.tracks.push_back(track);
        SynthPlan::Options options;
        options.cacheDirectory = root() / "cache";
        options.outputFile = root() / "out.wav";

        const auto curves = [&](bool mode2) {
            project.settings.mode2 = mode2;
            DiagnosticList diagnostics;
            const auto plan = SynthPlan::make(project, *voices, options, diagnostics);
            const auto &step = plan->steps().at(1);
            PitchCurve::Timing timing;
            timing.preUtterance = step.preUtterance;
            timing.startPoint = step.startPoint;
            const PitchCurve curve(project.tracks[0].notes, 1, 120);
            return std::make_tuple(step.pitch, curve.mode1Values(timing), curve.values(timing));
        };
        const auto [offPitch, offMode1, offMode2] = curves(false);
        QCOMPARE(offPitch, offMode1);
        QVERIFY(offPitch != offMode2);
        const auto [onPitch, onMode1, onMode2] = curves(true);
        QCOMPARE(onPitch, onMode2);
    }

    // A vibrato of 50 ms or less is left out on its own note but reaches into the next.
    void a_short_vibrato_only_reaches_into_the_next_note() {
        auto first = note(QStringLiteral("a"), 60, 240);
        Vibrato v;
        v.length = 20; // 48 ticks, 50 ms at 120 bpm
        v.period = 20;
        v.amplitude = 50;
        v.attack = 0;
        v.release = 0;
        first.vibrato = v;
        auto second = note(QStringLiteral("ka"), 60);
        second.portamento = {point(-40, 0), point(0, 0)};
        const QList<Note> notes = {first, second};

        const PitchCurve own(notes, 0, 120);
        for (double tick = 193; tick < 240; tick += 3) {
            QCOMPARE(own.vibratoAt(tick), 0.0);
        }
        const PitchCurve next(notes, 1, 120);
        bool reached = false;
        for (double tick = -47; tick < 0; tick += 3) {
            reached = reached || next.vibratoAt(tick) != 0;
        }
        QVERIFY(reached);
    }
};

QTEST_APPLESS_MAIN(test_PitchCurve)

#include "test_PitchCurve.moc"
