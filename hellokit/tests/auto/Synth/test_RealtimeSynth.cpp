#include <atomic>
#include <mutex>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSet>
#include <QtCore/QTemporaryDir>
#include <QtCore/QThread>
#include <QtTest/QTest>

#include <hellokit/Synth/RealtimeSynth.h>
#include <hellokit/Synth/SynthPlan.h>
#include <hellokit/Synth/WavtoolMixer.h>
#include <hellokit/VoiceBank/VoiceBank.h>

using namespace hello::kit;
namespace fs = std::filesystem;

namespace {

    // A second of direct current at \a level, 16-bit mono at 44100 Hz
    QByteArray fragment(qint16 level) {
        const quint32 bytes = 44100 * 2;
        QByteArray wave("RIFF");
        const auto u32 = [&wave](quint32 value) {
            for (int i = 0; i < 4; ++i) {
                wave.append(char((value >> (8 * i)) & 0xff));
            }
        };
        const auto u16 = [&wave](quint16 value) {
            wave.append(char(value & 0xff));
            wave.append(char(value >> 8));
        };
        u32(36 + bytes);
        wave.append("WAVEfmt ");
        u32(16);
        u16(1);
        u16(1);
        u32(44100);
        u32(44100 * 2);
        u16(2);
        u16(16);
        wave.append("data");
        u32(bytes);
        for (quint32 i = 0; i < bytes / 2; ++i) {
            u16(quint16(level));
        }
        return wave;
    }

    // What the stand-in resamplers did, shared by all of them
    struct Record {
        std::mutex mutex;
        QList<int> rendered;           // note numbers in the order rendered
        QSet<int> failing;             // note numbers that produce nothing
        std::atomic<bool> held{false}; // while set, every call waits
    };

    // Writes a fragment whose level identifies the note, taken from the number at the start of
    // the fragment name, instead of running a resampler.
    class StandIn : public EngineProcess {
    public:
        explicit StandIn(Record &record) : m_record(record) {
        }

        EngineRun run(const fs::path &program, const QStringList &arguments,
                      DiagnosticList &diagnostics) const override {
            Q_UNUSED(program);
            Q_UNUSED(diagnostics);
            while (m_record.held.load()) {
                QThread::msleep(2);
            }
            const auto output = fs::path(arguments.at(1).toStdU16String());
            const int note = QString::fromStdU16String(output.filename().u16string())
                                 .section(QLatin1Char('_'), 0, 0)
                                 .toInt();
            {
                const std::lock_guard lock(m_record.mutex);
                m_record.rendered.push_back(note);
                if (m_record.failing.contains(note)) {
                    return {true, 1, false, {}};
                }
            }
            QFile file(QString::fromStdU16String(output.u16string()));
            if (file.open(QIODevice::WriteOnly)) {
                file.write(fragment(qint16(1000 * (note + 1))));
            }
            EngineRun run;
            run.started = true;
            return run;
        }

    private:
        Record &m_record;
    };

}

class test_RealtimeSynth : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    Record m_record;

    fs::path root() const {
        return fs::path(m_dir->path().toStdU16String());
    }

    void write(const QString &relative, const QByteArray &bytes) {
        const QString path = m_dir->path() + QLatin1Char('/') + relative;
        QDir().mkpath(QFileInfo(path).path());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(bytes);
    }

    // Written once per test: the fragment names include the state of the sample files, so
    // writing them again would change every name.
    std::optional<VoiceBank> bank() {
        if (QFile::exists(m_dir->path() + QStringLiteral("/bank/oto.ini"))) {
            FixedCharsetSelector selector(QStringLiteral("UTF-8"));
            DiagnosticList diagnostics;
            return VoiceBank::open(root() / "bank", &selector, diagnostics);
        }
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

    // Five notes, of which the fourth is a rest
    static Project fiveNotes(const QString &flagsOfTheThird = {}) {
        QList<Note> notes;
        for (const auto lyric : {"a", "ka", "ki", "R", "a"}) {
            Note note;
            note.lyric = QString::fromLatin1(lyric);
            note.noteNum = 60;
            note.length = 480;
            notes.push_back(note);
        }
        notes[2].flags = flagsOfTheThird;
        Track track;
        track.notes = notes;
        Project project;
        project.tracks.push_back(track);
        return project;
    }

    std::optional<SynthPlan> planOf(const Project &project) {
        const auto voices = bank();
        if (!voices) {
            return std::nullopt;
        }
        SynthPlan::Options options;
        options.cacheDirectory = root() / "cache";
        options.outputFile = root() / "unused.wav";
        DiagnosticList diagnostics;
        return SynthPlan::make(project, *voices, options, diagnostics);
    }

    std::unique_ptr<RealtimeSynth> synth(int threads = 1) {
        return std::make_unique<RealtimeSynth>(
            SynthEngines{"resampler.exe", "wavtool.exe"}, threads,
            [this] { return std::make_unique<StandIn>(m_record); });
    }

    static QList<WavtoolMixer::Segment> segmentsOf(const SynthPlan &plan) {
        QList<WavtoolCall> calls;
        for (const auto &step : plan.steps()) {
            calls.push_back(WavtoolCall::parse(step.wavtoolArguments).value_or(WavtoolCall()));
        }
        return WavtoolMixer::layOut(calls);
    }

    QList<int> rendered() {
        const std::lock_guard lock(m_record.mutex);
        return m_record.rendered;
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        const std::lock_guard lock(m_record.mutex);
        m_record.rendered.clear();
        m_record.failing.clear();
        m_record.held = false;
    }

    // The notes from the position on come first, nearest first, then those before it, nearest
    // first; the rest is not rendered.
    void notes_are_rendered_from_the_playback_position() {
        const auto plan = planOf(fiveNotes());
        QVERIFY(plan);
        const auto segments = segmentsOf(*plan);
        const auto rt = synth();
        // In the middle of the third note, after the second has ended
        rt->setPosition(segments[2].start + segments[2].length / 2);
        rt->setPlan(*plan);
        QVERIFY(rt->waitReady(0, rt->length(), std::chrono::seconds(10)));
        QCOMPARE(rendered(), (QList<int>{2, 4, 1, 0}));
        QCOMPARE(rt->pendingCount(), 0);
        QCOMPARE(rt->startOf(2), segments[2].start);
        QCOMPARE(rt->startOf(9), qint64(0));
    }

    // Nothing is mixed before its notes are rendered, and the mix is that of the fragments.
    void a_part_is_mixed_once_its_notes_are_ready() {
        const auto plan = planOf(fiveNotes());
        QVERIFY(plan);
        const auto rt = synth();
        m_record.held = true;
        rt->setPlan(*plan);
        std::vector<qint16> out(1000);
        QVERIFY(!rt->isReady(0, 1000));
        QVERIFY(!rt->mix(0, 1000, out.data()));
        QVERIFY(!rt->waitReady(0, 1000, std::chrono::milliseconds(20)));
        m_record.held = false;
        QVERIFY(rt->waitReady(0, rt->length(), std::chrono::seconds(10)));

        const auto segments = segmentsOf(*plan);
        QCOMPARE(rt->length(), WavtoolMixer::lengthOf(segments));
        const std::vector<qint16> levels[] = {std::vector<qint16>(44100, 1000),
                                              std::vector<qint16>(44100, 2000),
                                              std::vector<qint16>(44100, 3000),
                                              {},
                                              std::vector<qint16>(44100, 5000)};
        std::vector<qint16> expected(size_t(rt->length()));
        WavtoolMixer::mix(
            segments,
            [&levels](int index) { return levels[index].empty() ? nullptr : &levels[index]; }, 0,
            qint64(expected.size()), expected.data());
        std::vector<qint16> whole(expected.size());
        QVERIFY(rt->mix(0, qint64(whole.size()), whole.data()));
        QCOMPARE(whole, expected);
        QCOMPARE(rt->startTime(), plan->startTime());
    }

    // A note changed by an edit is rendered again; the others keep their fragments, in memory
    // and in the cache directory.
    void only_changed_notes_are_rendered_again() {
        const auto rt = synth(2);
        const auto first = planOf(fiveNotes());
        QVERIFY(first);
        rt->setPlan(*first);
        QVERIFY(rt->waitReady(0, rt->length(), std::chrono::seconds(10)));
        QCOMPARE(rendered().size(), 4);

        const auto second = planOf(fiveNotes(QStringLiteral("g-5")));
        QVERIFY(second);
        rt->setPlan(*second);
        QVERIFY(rt->waitReady(0, rt->length(), std::chrono::seconds(10)));
        QCOMPARE(rendered().size(), 5);
        QCOMPARE(rendered().last(), 2);

        // Another instance reads what is in the cache instead of rendering it.
        const auto again = synth();
        again->setPlan(*second);
        QVERIFY(again->waitReady(0, again->length(), std::chrono::seconds(10)));
        QCOMPARE(rendered().size(), 5);
    }

    // The fragments kept in memory survive a new plan, even if the cache directory is emptied.
    void fragments_in_memory_survive_a_new_plan() {
        const auto rt = synth();
        const auto first = planOf(fiveNotes());
        QVERIFY(first);
        rt->setPlan(*first);
        QVERIFY(rt->waitReady(0, rt->length(), std::chrono::seconds(10)));
        QCOMPARE(rendered().size(), 4);

        std::error_code error;
        fs::remove_all(root() / "cache", error);
        const auto second = planOf(fiveNotes(QStringLiteral("g-5")));
        QVERIFY(second);
        rt->setPlan(*second);
        QVERIFY(rt->waitReady(0, rt->length(), std::chrono::seconds(10)));
        QCOMPARE(rendered(), (QList<int>{0, 1, 2, 4, 2}));
    }

    // Each note tells how far its fragment has come, and the rest that it has none.
    void each_note_tells_how_far_it_is_rendered() {
        using S = RealtimeSynth;
        const auto plan = planOf(fiveNotes());
        QVERIFY(plan);
        m_record.failing.insert(1);
        m_record.held = true;
        const auto rt = synth();
        rt->setPlan(*plan);
        QTRY_COMPARE(rt->noteStates(), (QList<S::NoteState>{S::Running, S::Waiting, S::Waiting,
                                                            S::Silent, S::Waiting}));
        m_record.held = false;
        QVERIFY(rt->waitReady(0, rt->length(), std::chrono::seconds(10)));
        QCOMPARE(rt->noteStates(),
                 (QList<S::NoteState>{S::Ready, S::Failed, S::Ready, S::Silent, S::Ready}));
    }

    // A note the resampler cannot render is silent, with a warning, and does not hold back
    // playback.
    void a_failed_note_is_silent() {
        const auto plan = planOf(fiveNotes());
        QVERIFY(plan);
        m_record.failing.insert(1);
        const auto rt = synth();
        rt->setPlan(*plan);
        QVERIFY(rt->waitReady(0, rt->length(), std::chrono::seconds(10)));
        const auto diagnostics = rt->takeDiagnostics();
        QCOMPARE(diagnostics.size(), 1);
        QCOMPARE(diagnostics.front().severity, DiagnosticSeverity::Warning);
        QCOMPARE(diagnostics.front().noteIndex, std::optional<int>(1));
        QVERIFY(rt->takeDiagnostics().isEmpty());

        const auto segments = segmentsOf(*plan);
        std::vector<qint16> out(100);
        const qint64 middle = segments[1].start + segments[1].length / 2;
        QVERIFY(rt->mix(middle, 100, out.data()));
        QCOMPARE(out.front(), qint16(0));
    }
};

QTEST_GUILESS_MAIN(test_RealtimeSynth)

#include "test_RealtimeSynth.moc"
