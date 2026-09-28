#include <memory>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QRandomGenerator>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Document/TempoMap.h>
#include <hellokit/Synth/SampleTiming.h>
#include <hellokit/Synth/SynthPlan.h>

using namespace hello::kit;

class test_SampleTiming : public QObject {
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

    // Samples whose pre-utterances are long enough to be shortened after short notes, one of
    // them with an overlap beyond its pre-utterance
    std::optional<VoiceBank> bank() {
        write(QStringLiteral("bank/oto.ini"), "a.wav=a,10,20,30,40,5\n"
                                              "ka.wav=ka,11,21,31,160,20\n"
                                              "ki.wav=ki,12,22,32,120,60\n"
                                              "ko.wav=ko,13,23,33,30,80\n");
        for (const auto name : {"a", "ka", "ki", "ko"}) {
            write(QStringLiteral("bank/%1.wav").arg(QLatin1String(name)), "RIFF");
        }
        FixedCharsetSelector selector(QStringLiteral("UTF-8"));
        DiagnosticList diagnostics;
        return VoiceBank::open(root() / "bank", &selector, diagnostics);
    }

    // Notes long and short, rests, lyrics without samples, tempo changes, and the values of a
    // note that replace or scale those of its sample
    static QList<Note> randomNotes(quint32 seed) {
        QRandomGenerator random(seed);
        const auto between = [&random](int low, int high) { return random.bounded(low, high + 1); };
        const QStringList lyrics = {QStringLiteral("a"),  QStringLiteral("ka"),
                                    QStringLiteral("ki"), QStringLiteral("ko"),
                                    QStringLiteral("R"),  QStringLiteral("xy")};
        QList<Note> notes;
        for (int i = 0; i < 40; ++i) {
            Note note;
            note.lyric = lyrics.at(between(0, 5));
            note.noteNum = between(50, 72);
            note.length = between(0, 2) == 0 ? between(10, 120) : between(120, 960);
            if (between(0, 4) == 0) {
                note.tempo = between(60, 240);
            }
            if (between(0, 4) == 0) {
                note.velocity = between(0, 200);
            }
            if (between(0, 5) == 0) {
                note.preUtterance = between(0, 300);
            }
            if (between(0, 5) == 0) {
                note.voiceOverlap = between(-50, 150);
            }
            if (between(0, 5) == 0) {
                note.startPoint = between(0, 80);
            }
            notes.push_back(note);
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

    // The purpose of the class: the timing that places an envelope is that of the synthesis.
    void the_timing_is_that_of_the_synthesis() {
        const auto voices = bank();
        QVERIFY(voices);
        for (quint32 seed = 1; seed <= 20; ++seed) {
            Project project;
            project.settings.tempo = 120;
            Track track;
            track.notes = randomNotes(seed);
            project.tracks.push_back(track);

            SynthPlan::Options options;
            options.cacheDirectory = root() / "cache";
            options.outputFile = root() / "out.wav";
            DiagnosticList diagnostics;
            const auto plan = SynthPlan::make(project, *voices, options, diagnostics);
            QVERIFY(plan);

            const auto timings = SampleTiming::of(track.notes, TempoMap::of(project), &*voices);
            QCOMPARE(timings.size(), plan->steps().size());
            for (qsizetype i = 0; i < timings.size(); ++i) {
                const auto &step = plan->steps()[i];
                if (timings[i].preUtterance != step.preUtterance ||
                    timings[i].voiceOverlap != step.voiceOverlap ||
                    timings[i].startPoint != step.startPoint) {
                    QFAIL(qPrintable(QStringLiteral("seed %1, note %2").arg(seed).arg(i)));
                }
            }
        }
    }

    // After a short sung note the pre-utterance takes half of it, after a rest all of it.
    void a_short_note_before_shortens_the_sample() {
        const auto voices = bank();
        QVERIFY(voices);
        Note before;
        before.lyric = QStringLiteral("a");
        before.length = 120; // 125 ms at 120 bpm
        Note note;
        note.lyric = QStringLiteral("ka"); // 160 ms before, 20 ms of overlap
        note.length = 480;
        // The tempos of notes at 120 bpm
        const auto tempos = [](const QList<Note> &notes) {
            TempoMap map(120);
            for (const auto &n : notes) {
                map.append(n.length, n.tempo);
            }
            return map;
        };

        const auto sung = SampleTiming::of({before, note}, tempos({before, note}), &*voices);
        // 140 ms of pre-utterance beyond the overlap fit into 62.5 ms.
        const double rate = 62.5 / 140;
        QCOMPARE(sung[1].preUtterance, 160 * rate);
        QCOMPARE(sung[1].voiceOverlap, 20 * rate);
        QCOMPARE(sung[1].startPoint, 160 - 160 * rate);

        before.lyric = QStringLiteral("R");
        const auto rest = SampleTiming::of({before, note}, tempos({before, note}), &*voices);
        QCOMPARE(rest[1].preUtterance, 160 * (125 / 140.0));

        // Without a bank, the values of the note alone
        note.preUtterance = 30;
        const auto bare = SampleTiming::of({note}, tempos({note}), nullptr);
        QCOMPARE(bare[0].preUtterance, 30.0);
        QCOMPARE(bare[0].voiceOverlap, 0.0);
    }
};

QTEST_APPLESS_MAIN(test_SampleTiming)

#include "test_SampleTiming.moc"
