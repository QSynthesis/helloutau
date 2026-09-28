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
    qsizetype firstDifference(const VoiceBank &voices, const QList<Note> &notes, double tempo) {
        Project project;
        project.settings.tempo = tempo;
        Track track;
        track.notes = notes;
        project.tracks.push_back(track);

        SynthPlan::Options options;
        options.cacheDirectory = root() / "cache";
        options.outputFile = root() / "out.wav";
        DiagnosticList diagnostics;
        const auto plan = SynthPlan::make(project, voices, options, diagnostics);
        if (!plan) {
            return 0;
        }

        const auto tempos = TempoMap::of(project);
        const auto &steps = plan->steps();
        for (qsizetype i = 0; i < steps.size(); ++i) {
            PitchCurve::Timing timing;
            timing.preUtterance = steps[i].preUtterance;
            timing.startPoint = steps[i].startPoint;
            if (i + 1 < steps.size()) {
                timing.nextPreUtterance = steps[i + 1].preUtterance;
                timing.nextOverlap = steps[i + 1].voiceOverlap;
            }
            if (PitchCurve(notes, i, tempos.tempo(int(i))).values(timing) != steps[i].pitch) {
                return i;
            }
        }
        return -1;
    }

    // The purpose of the class: the curve drawn is the curve the resampler receives.
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
