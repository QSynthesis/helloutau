/// \file
/// Covers the behavior of a render after the arguments are passed to the engines.
///
/// The engines are third-party programs outside this repository.
/// \c SynthRunner::makeEngineProcess() is the test seam that makes this behavior testable:
/// \c StandIn below is a substitute engine that behaves as the test specifies and records its
/// invocations.
///
/// A substitute cannot verify whether the arguments are *correct*, which requires the real
/// engines. That is covered by \c tests/manual/utaucompare , which compares against the UTAU
/// render of the same project.

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

    /// The invocations of the substitute engine, stored so that they outlive the engine.
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

    /// A substitute engine that behaves as the test specifies.
    ///
    /// \note One instance is shared by all threads of the runner, so its state is protected by
    ///       the lock of the log.
    class StandIn : public EngineProcess {
    public:
        /// Called with the engine arguments. Returning false simulates an engine that fails to
        /// start.
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
            // Outside the lock, because a resampler writes a file, and the threads are intended
            // to do so concurrently.
            out.started = what && *what ? (*what)(arguments) : bool(what);
            return out;
        }

    private:
        EngineLog *m_log;
        SynthEngines m_which;
        Behaviour m_resampler;
        Behaviour m_wavtool;
    };

    /// A runner that starts the substitute engine supplied by the test.
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

    /// Nonexistent engine paths, because the substitute handles the calls.
    SynthEngines somewhere() const {
        SynthEngines engines;
        engines.resampler = root() / "nowhere" / "resampler";
        engines.wavtool = root() / "nowhere" / "wavtool";
        return engines;
    }

    /// A resampler that writes the requested fragment.
    static StandIn::Behaviour rendersTo(const std::filesystem::path &piece) {
        return [piece](const QStringList &) {
            std::ofstream out(piece, std::ios::binary | std::ios::trunc);
            out << "a rendered piece";
            return bool(out);
        };
    }

    /// A resampler that starts, reports nothing and writes nothing.
    static StandIn::Behaviour writesNothing() {
        return [](const QStringList &) { return true; };
    }

    /// A wavtool that writes the header and the sample data separately, as the UTAU wavtool
    /// does, and appends.
    static StandIn::Behaviour appends(const std::filesystem::path &track) {
        return [track](const QStringList &) {
            std::ofstream header(withSuffix(track, ".whd"), std::ios::binary | std::ios::app);
            header << std::string(44, 'H');
            std::ofstream data(withSuffix(track, ".dat"), std::ios::binary | std::ios::app);
            data << std::string(100, 'D');
            return bool(header) && bool(data);
        };
    }

    /// A plan for \a notes notes, sufficient for every test here.
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
            note.noteNum = 60 + i; // so that each note has its own fragment
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

    // The wavtool does not write a WAV file. It writes the header and the sample data to two
    // files beside the track, <out>.whd and <out>.dat , and joining them is the final step of a
    // render. A render that stops earlier leaves two unplayable files.
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

    // The wavtool appends, so a render must remove both files before it starts. Otherwise its
    // output is appended to existing content. A completed render removes the files itself, so
    // leftover files originate only from an interrupted render, which this test recreates.
    //
    // The test creates the leftover files directly rather than rendering twice, because the
    // cleanup of the first render would leave nothing to remove, and the test would then pass
    // even without the removal step.
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

    // The complete cycle: rendering the same project again, with every fragment reused,
    // produces the same track rather than a longer one.
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

        // The second render did not render the note again, because the note is unchanged.
        QCOMPARE(log.resampled(), 1);
        QCOMPARE(log.appended(), 2);
    }

    // An engine that starts, reports nothing and writes nothing is the most common render
    // failure, caused for example by an unreadable sample or an unknown flag. Exit codes are
    // unreliable, so success is determined by the existence of the fragment.
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

    // Disabled by default, because one defective sample should not prevent the entire track.
    // When enabled, the render must stop: no wavtool calls, no track file, and only the first
    // failure is reported.
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

    // Reuse is the only part of rendering observable without an engine, because reuse means
    // not starting one. The engine paths here are nonexistent, so a note reported as reused
    // cannot have been rendered.
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

    // The override for the case in which the engine itself has changed.
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

    // The cache directory belongs to the project and persists with it, so fragments rendered
    // for a note before an edit must be removed. Fragments of notes outside this render remain.
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

    // An engine missing from its configured location must be reported. Rendering nothing
    // without a report is a failure that is costly for the user to diagnose.
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

    // The engines write into the directory, so it must exist before they run. They do not
    // create it, and their diagnostic for a nonexistent path is engine-specific.
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
