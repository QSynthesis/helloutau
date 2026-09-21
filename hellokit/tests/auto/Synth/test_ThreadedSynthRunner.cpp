/// \file
/// Covers what a render does once it has handed the arguments over.
///
/// The engines are somebody else's programs and are not in this repository, so all of that used
/// to be out of reach. \c SynthRunner::makeEngineProcess() is the seam that puts it back:
/// \c StandIn below is an engine that does what the test says instead of what a real one would,
/// and writes down what it was asked for.
///
/// What a stand-in cannot say is whether the arguments were *right*, which is what the real
/// engines are for. That is \c tests/manual/utaucompare , against UTAU's own render of the same
/// project.

#include <fstream>
#include <functional>
#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QMutex>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Synth/ThreadedSynthRunner.h>

using namespace hello::kit;

namespace {

    /// What the stand-in engine was asked to do, kept where it outlives the engine itself.
    struct EngineLog {
        mutable QMutex lock;
        int resamplerCalls = 0;
        int wavtoolCalls = 0;

        int resampled() const {
            const QMutexLocker locked(&lock);
            return resamplerCalls;
        }

        int appended() const {
            const QMutexLocker locked(&lock);
            return wavtoolCalls;
        }
    };

    /// An engine that does what the test says instead of what a real one would.
    ///
    /// \note One of these is shared by every thread the runner uses, so what it keeps is kept
    ///       behind the log's lock.
    class StandIn : public EngineProcess {
    public:
        /// Called with the arguments the engine was handed. Returning false is an engine that
        /// would not start at all.
        using Behaviour = std::function<bool(const QStringList &)>;

        StandIn(EngineLog *log, const SynthEngines &which, Behaviour resampler, Behaviour wavtool)
            : m_log(log), m_which(which), m_resampler(std::move(resampler)),
              m_wavtool(std::move(wavtool)) {
        }

        EngineRun run(const std::filesystem::path &program, const QStringList &arguments,
                      DiagnosticList &) const override {
            EngineRun out;
            const Behaviour *what = nullptr;
            {
                const QMutexLocker locked(&m_log->lock);
                if (program == m_which.resampler) {
                    ++m_log->resamplerCalls;
                    what = &m_resampler;
                } else if (program == m_which.wavtool) {
                    ++m_log->wavtoolCalls;
                    what = &m_wavtool;
                }
            }
            // Outside the lock, because what a resampler does is write a file and the threads
            // are meant to do that at the same time.
            out.started = what && *what ? (*what)(arguments) : bool(what);
            return out;
        }

    private:
        EngineLog *m_log;
        SynthEngines m_which;
        Behaviour m_resampler;
        Behaviour m_wavtool;
    };

    /// A runner that starts the engine the test gave it rather than a real one.
    class StubbedRunner : public ThreadedSynthRunner {
    public:
        StubbedRunner(EngineLog *log, SynthEngines which, StandIn::Behaviour resampler,
                      StandIn::Behaviour wavtool)
            : m_log(log), m_which(std::move(which)), m_resampler(std::move(resampler)),
              m_wavtool(std::move(wavtool)) {
        }

    protected:
        std::unique_ptr<EngineProcess> makeEngineProcess() const override {
            return std::make_unique<StandIn>(m_log, m_which, m_resampler, m_wavtool);
        }

    private:
        EngineLog *m_log;
        SynthEngines m_which;
        StandIn::Behaviour m_resampler;
        StandIn::Behaviour m_wavtool;
    };

}

class test_ThreadedSynthRunner : public QObject {
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

    static std::filesystem::path withSuffix(const std::filesystem::path &path, const char *what) {
        return std::filesystem::path(path) += what;
    }

    static qint64 sizeOf(const std::filesystem::path &path) {
        std::error_code error;
        const auto size = std::filesystem::file_size(path, error);
        return error ? -1 : qint64(size);
    }

    /// Engine paths that are nowhere, since the stand-in is what answers to them.
    SynthEngines somewhere() const {
        SynthEngines engines;
        engines.resampler = root() / "nowhere" / "resampler";
        engines.wavtool = root() / "nowhere" / "wavtool";
        return engines;
    }

    /// A resampler that writes the piece it was asked for.
    static StandIn::Behaviour rendersTo(const std::filesystem::path &piece) {
        return [piece](const QStringList &) {
            std::ofstream out(piece, std::ios::binary | std::ios::trunc);
            out << "a rendered piece";
            return bool(out);
        };
    }

    /// A resampler that starts, says nothing and leaves nothing behind.
    static StandIn::Behaviour writesNothing() {
        return [](const QStringList &) { return true; };
    }

    /// A wavtool that keeps the header and the samples apart the way UTAU's does, and appends.
    static StandIn::Behaviour appends(const std::filesystem::path &track) {
        return [track](const QStringList &) {
            std::ofstream header(withSuffix(track, ".whd"), std::ios::binary | std::ios::app);
            header << std::string(44, 'H');
            std::ofstream data(withSuffix(track, ".dat"), std::ios::binary | std::ios::app);
            data << std::string(100, 'D');
            return bool(header) && bool(data);
        };
    }

    /// A plan for \a notes notes, which is as much as anything here needs.
    std::optional<SynthPlan> plan(int notes = 1) {
        write(QStringLiteral("bank/oto.ini"), "a.wav=a,10,20,30,40,5\n");
        write(QStringLiteral("bank/a.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("UTF-8"));
        DiagnosticList diagnostics;
        const auto bank = VoiceBank::open(root() / "bank", &selector, diagnostics);
        if (!bank) {
            return std::nullopt;
        }

        Project project;
        Track track;
        for (int i = 0; i < notes; ++i) {
            Note note;
            note.lyric = QStringLiteral("a");
            note.noteNum = 60 + i; // so that each one is its own piece
            note.length = 480;
            track.notes.push_back(note);
        }
        project.tracks.push_back(track);

        SynthPlan::Options options;
        options.cacheDirectory = root() / "cache";
        options.outputFile = root() / "out.wav";
        return SynthPlan::make(project, *bank, options, diagnostics);
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void cleanup() {
        m_dir.reset();
    }

    // The wavtool does not write a wav. It keeps the header and the samples in two files
    // beside the track, <out>.whd and <out>.dat , and joining them is the last thing a render
    // does. A render that stopped before that leaves two files nobody can play.
    void the_two_pieces_the_wavtool_writes_are_joined_and_cleared() {
        const auto p = plan();
        QVERIFY(p.has_value());

        EngineLog log;
        const auto engines = somewhere();
        StubbedRunner runner(&log, engines, rendersTo(p->steps().at(0).cacheFile),
                             appends(p->outputFile()));

        DiagnosticList diagnostics;
        const auto outcome = runner.render(*p, engines, nullptr, diagnostics);

        QCOMPARE(log.resampled(), 1);
        QCOMPARE(log.appended(), 1);
        QVERIFY(outcome.rendered);
        QVERIFY(std::filesystem::exists(p->outputFile()));
        QVERIFY(!std::filesystem::exists(withSuffix(p->outputFile(), ".whd")));
        QVERIFY(!std::filesystem::exists(withSuffix(p->outputFile(), ".dat")));
        QCOMPARE(sizeOf(p->outputFile()), 44 + 100);
    }

    // The wavtool appends, so a render has to clear the two pieces before it starts or it
    // lands on the end of whatever is there. A render that finished cleared them itself; what
    // leaves them behind is one that did not finish, which is what this puts back.
    //
    // Written this way after the first attempt turned out to prove nothing: it rendered twice
    // and the first render's own cleanup meant the second had nothing to clear, so taking the
    // clearing out did not make it fail.
    void a_render_clears_what_an_earlier_one_left_behind() {
        const auto p = plan();
        QVERIFY(p.has_value());

        write(QStringLiteral("out.wav.whd"), QByteArray(44, 'X'));
        write(QStringLiteral("out.wav.dat"), QByteArray(999, 'X'));

        EngineLog log;
        const auto engines = somewhere();
        StubbedRunner runner(&log, engines, rendersTo(p->steps().at(0).cacheFile),
                             appends(p->outputFile()));

        DiagnosticList diagnostics;
        QVERIFY(runner.render(*p, engines, nullptr, diagnostics).rendered);
        QCOMPARE(sizeOf(p->outputFile()), 44 + 100);
    }

    // And the whole cycle: rendering the same project again, which reuses every piece, comes
    // out as the same track rather than a longer one.
    void a_second_render_comes_out_the_same_length() {
        const auto p = plan();
        QVERIFY(p.has_value());

        EngineLog log;
        const auto engines = somewhere();
        StubbedRunner runner(&log, engines, rendersTo(p->steps().at(0).cacheFile),
                             appends(p->outputFile()));

        DiagnosticList diagnostics;
        QVERIFY(runner.render(*p, engines, nullptr, diagnostics).rendered);
        const auto first = sizeOf(p->outputFile());

        QVERIFY(runner.render(*p, engines, nullptr, diagnostics).rendered);
        QCOMPARE(sizeOf(p->outputFile()), first);

        // And the second one did not render it again, since nothing about the note changed.
        QCOMPARE(log.resampled(), 1);
        QCOMPARE(log.appended(), 2);
    }

    // An engine that starts, says nothing and writes nothing is the common way for a render to
    // go wrong: a sample the resampler cannot read, a flag it does not know. Exit codes do not
    // settle it, so what settles it is whether the piece appeared.
    void a_note_whose_piece_never_appeared_is_reported() {
        const auto p = plan();
        QVERIFY(p.has_value());

        EngineLog log;
        const auto engines = somewhere();
        StubbedRunner runner(&log, engines, writesNothing(), appends(p->outputFile()));

        DiagnosticList diagnostics;
        const auto outcome = runner.render(*p, engines, nullptr, diagnostics);

        QCOMPARE(log.resampled(), 1);
        QCOMPARE(outcome.resampled, 0);
        QCOMPARE(outcome.failed, 1);
        QVERIFY(hasError(diagnostics));
    }

    // Off by default, because one bad sample should not cost the whole track. On, it has to
    // actually stop: no wavtool calls, no track file, and the report names the first one only.
    void stop_on_first_failure_stops() {
        const auto p = plan(3);
        QVERIFY(p.has_value());
        QCOMPARE(p->steps().size(), 3);

        EngineLog log;
        const auto engines = somewhere();
        StubbedRunner runner(&log, engines, writesNothing(), appends(p->outputFile()));
        runner.stopOnFirstFailure = true;

        DiagnosticList diagnostics;
        const auto outcome = runner.render(*p, engines, nullptr, diagnostics);

        QCOMPARE(outcome.failed, 1);
        QCOMPARE(log.appended(), 0);
        QVERIFY(!outcome.rendered);
        QVERIFY(!std::filesystem::exists(p->outputFile()));
    }

    // Reuse is the one part of rendering a test can see without an engine, because reusing is
    // exactly not starting one: the engines here point at nothing, so a note that comes back
    // reused can only have come back that way by being left alone.
    void a_piece_already_there_is_not_rendered_again() {
        const auto p = plan();
        QVERIFY(p.has_value());
        QCOMPARE(p->steps().size(), 1);

        QVERIFY(QDir().mkpath(QString::fromStdU16String(p->cacheDirectory().u16string())));
        write(QStringLiteral("cache/") +
                  QString::fromStdU16String(p->steps().at(0).cacheFile.filename().u16string()),
              "RIFF already rendered");

        SynthEngines engines;
        engines.resampler = root() / "nowhere" / "resampler.exe";
        engines.wavtool = root() / "nowhere" / "wavtool.exe";

        DiagnosticList diagnostics;
        const ThreadedSynthRunner runner;
        const auto outcome = runner.render(*p, engines, nullptr, diagnostics);

        QCOMPARE(outcome.reused, 1);
        QCOMPARE(outcome.resampled, 0);
        QCOMPARE(outcome.failed, 0);
    }

    // And the way out, for when the engine itself is what changed.
    void turning_reuse_off_renders_it_again() {
        const auto p = plan();
        QVERIFY(p.has_value());

        QVERIFY(QDir().mkpath(QString::fromStdU16String(p->cacheDirectory().u16string())));
        write(QStringLiteral("cache/") +
                  QString::fromStdU16String(p->steps().at(0).cacheFile.filename().u16string()),
              "RIFF already rendered");

        SynthEngines engines;
        engines.resampler = root() / "nowhere" / "resampler.exe";
        engines.wavtool = root() / "nowhere" / "wavtool.exe";

        DiagnosticList diagnostics;
        ThreadedSynthRunner runner;
        runner.reuseCache = false;
        const auto outcome = runner.render(*p, engines, nullptr, diagnostics);

        QCOMPARE(outcome.reused, 0);
    }

    // The folder is the project's and lives as long as it does, so the pieces a note rendered to
    // before it was edited have to go. What belongs to notes this render is not touching stays.
    void the_pieces_a_note_no_longer_wants_are_cleared() {
        const auto p = plan();
        QVERIFY(p.has_value());

        const QString cache = QStringLiteral("cache/");
        const QString wanted =
            QString::fromStdU16String(p->steps().at(0).cacheFile.filename().u16string());
        write(cache + QStringLiteral("0_a_C4_gone00.wav"), "an older take of this note");
        write(cache + QStringLiteral("7_a_C4_stays0.wav"), "a note this render is not touching");
        write(cache + QStringLiteral("notes.txt"), "not a piece at all");
        write(cache + wanted, "RIFF already rendered");

        SynthEngines engines;
        engines.resampler = root() / "nowhere" / "resampler.exe";
        engines.wavtool = root() / "nowhere" / "wavtool.exe";

        DiagnosticList diagnostics;
        const ThreadedSynthRunner runner;
        runner.render(*p, engines, nullptr, diagnostics);

        const auto there = [&](const QString &name) {
            return std::filesystem::exists(p->cacheDirectory() /
                                           std::filesystem::u8path(name.toStdString()));
        };
        QVERIFY(!there(QStringLiteral("0_a_C4_gone00.wav")));
        QVERIFY(there(QStringLiteral("7_a_C4_stays0.wav")));
        QVERIFY(there(QStringLiteral("notes.txt")));
        QVERIFY(there(wanted));
    }

    // An engine that is not where it was said to be has to be said out loud. Rendering nothing
    // and reporting nothing is the failure that would waste a user's afternoon.
    void engines_that_are_not_there_are_reported() {
        const auto p = plan();
        QVERIFY(p.has_value());

        SynthEngines engines;
        engines.resampler = root() / "nowhere" / "resampler.exe";
        engines.wavtool = root() / "nowhere" / "wavtool.exe";

        DiagnosticList diagnostics;
        const ThreadedSynthRunner runner;
        const auto outcome = runner.render(*p, engines, nullptr, diagnostics);

        QVERIFY(!outcome.rendered);
        QCOMPARE(outcome.resampled, 0);
        QCOMPARE(outcome.failed, 1);
        QVERIFY(hasError(diagnostics));
        QVERIFY(!std::filesystem::exists(p->outputFile()));
    }

    // The engines write into it, so it has to be there before they run. They are not going to
    // create it, and the diagnostic they give for a path that does not exist is their own.
    void the_cache_folder_is_created() {
        const auto p = plan();
        QVERIFY(p.has_value());
        QVERIFY(!std::filesystem::exists(p->cacheDirectory()));

        SynthEngines engines;
        engines.resampler = root() / "nowhere" / "resampler.exe";
        engines.wavtool = root() / "nowhere" / "wavtool.exe";

        DiagnosticList diagnostics;
        const ThreadedSynthRunner runner;
        runner.render(*p, engines, nullptr, diagnostics);

        QVERIFY(std::filesystem::is_directory(p->cacheDirectory()));
    }
};

QTEST_APPLESS_MAIN(test_ThreadedSynthRunner)

#include "test_ThreadedSynthRunner.moc"
