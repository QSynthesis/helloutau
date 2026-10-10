/// \file
/// Covers the behavior of a render after the arguments are passed to the synth tools.
///
/// The synth tools are third-party programs outside this repository.
/// \c SynthRunner::makeSynthToolProcess() is the test seam that makes this behavior testable:
/// \c StandIn below is a substitute synth tool that behaves as the test specifies and records its
/// invocations.
///
/// A substitute cannot verify whether the arguments are *correct*, which requires the real
/// synth tools. That is covered by \c tests/manual/utaucompare , which compares against the UTAU
/// render of the same project.

#include <atomic>
#include <chrono>
#include <fstream>
#include <functional>
#include <memory>
#include <string_view>
#include <thread>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QMutex>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Support/TextCodec.h>
#include <hellokit/Synth/ClassicSynthRunner.h>
#include <hellokit/Synth/ThreadedSynthRunner.h>

using namespace hello::kit;

namespace {

    /// The invocations of the substitute synth tool, stored so that they outlive the synth tool.
    struct SynthToolLog {
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

    /// A substitute synth tool that behaves as the test specifies.
    ///
    /// \note One instance is shared by all threads of the runner, so its state is protected by
    ///       the lock of the log.
    class StandIn : public SynthToolProcess {
    public:
        /// Called with the synth tool arguments. Returning false simulates a synth tool that fails
        /// to start.
        using Behaviour = std::function<bool(const QStringList &)>;

        StandIn(SynthToolLog *log, const SynthTools &which, Behaviour resampler, Behaviour wavtool)
            : m_log(log), m_which(which), m_resampler(std::move(resampler)),
              m_wavtool(std::move(wavtool)) {
        }

        SynthToolRun run(const std::filesystem::path &program, const QStringList &arguments,
                         DiagnosticList &, const std::function<bool()> &) const override {
            SynthToolRun out;
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
        SynthToolLog *m_log;
        SynthTools m_which;
        Behaviour m_resampler;
        Behaviour m_wavtool;
    };

    /// Records the progress reports and requests cancellation once \c cancelAt steps are
    /// complete.
    ///
    /// \note The runner serializes the calls of its observer, so no lock is required.
    class Recorder : public SynthObserver {
    public:
        QList<std::pair<int, int>> reports;
        int cancelAt = -1;

        void progressed(int done, int total) override {
            reports.push_back({done, total});
        }

        bool cancelled() override {
            return cancelAt >= 0 && !reports.isEmpty() && reports.last().first >= cancelAt;
        }
    };

    /// A runner that starts the substitute synth tool supplied by the test.
    class StubbedRunner : public ThreadedSynthRunner {
    public:
        StubbedRunner(SynthToolLog *log, SynthTools which, StandIn::Behaviour resampler,
                      StandIn::Behaviour wavtool)
            : m_log(log), m_which(std::move(which)), m_resampler(std::move(resampler)),
              m_wavtool(std::move(wavtool)) {
        }

    protected:
        std::unique_ptr<SynthToolProcess> makeSynthToolProcess() const override {
            return std::make_unique<StandIn>(m_log, m_which, m_resampler, m_wavtool);
        }

    private:
        SynthToolLog *m_log;
        SynthTools m_which;
        StandIn::Behaviour m_resampler;
        StandIn::Behaviour m_wavtool;
    };

    /// A substitute synth tool whose behaviour receives the cancellation query and returns the
    /// whole result, for the cases of a cancelled or timed-out call. The behaviour runs on the
    /// threads of the runner.
    class PollingStandIn : public SynthToolProcess {
    public:
        using Behaviour = std::function<SynthToolRun(const std::filesystem::path &program,
                                                     const QStringList &arguments,
                                                     const std::function<bool()> &cancelled)>;

        explicit PollingStandIn(Behaviour behaviour) : m_behaviour(std::move(behaviour)) {
        }

        SynthToolRun run(const std::filesystem::path &program, const QStringList &arguments,
                         DiagnosticList &, const std::function<bool()> &cancelled) const override {
            return m_behaviour(program, arguments, cancelled);
        }

    private:
        Behaviour m_behaviour;
    };

    class PollingRunner : public ThreadedSynthRunner {
    public:
        explicit PollingRunner(PollingStandIn::Behaviour behaviour)
            : m_behaviour(std::move(behaviour)) {
        }

    protected:
        std::unique_ptr<SynthToolProcess> makeSynthToolProcess() const override {
            return std::make_unique<PollingStandIn>(m_behaviour);
        }

    private:
        PollingStandIn::Behaviour m_behaviour;
    };

    /// Returns the fragment that the resampler arguments \a arguments request.
    std::filesystem::path fragmentOf(const QStringList &arguments) {
        return std::filesystem::path(arguments.at(1).toStdU16String());
    }

    /// Waits until \a cancelled returns true, for at most five seconds, and returns whether it
    /// did.
    bool waitForCancellation(const std::function<bool()> &cancelled) {
        for (int i = 0; i < 500; ++i) {
            if (cancelled && cancelled()) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }

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

    /// Nonexistent synth tool paths, because the substitute handles the calls.
    SynthTools somewhere() const {
        SynthTools synthTools;
        synthTools.resampler = root() / "nowhere" / "resampler";
        synthTools.wavtool = root() / "nowhere" / "wavtool";
        return synthTools;
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

    /// The content that writesTrack() writes.
    static constexpr std::string_view directTrack = "RIFF direct output";

    /// A wavtool that writes the completed track directly, as moresampler does in this role.
    static StandIn::Behaviour writesTrack(const std::filesystem::path &track) {
        return [track](const QStringList &) {
            std::ofstream out(track, std::ios::binary | std::ios::trunc);
            out << directTrack;
            return bool(out);
        };
    }

    /// Writes a fragment for every note of \a plan into the cache. The cache is then in the state
    /// that an earlier render leaves.
    static void fillCache(const SynthPlan &plan) {
        std::filesystem::create_directories(plan.cacheDirectory());
        for (const auto &step : plan.steps()) {
            std::ofstream out(step.cacheFile, std::ios::binary | std::ios::trunc);
            out << "RIFF already rendered";
        }
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

    /// A plan of one note whose fragment lies in the cache directory \a cacheName.
    std::optional<SynthPlan> planWithCache(const QString &cacheName) {
        write(QStringLiteral("bank/oto.ini"), "a.wav=a,10,20,30,40,5\n");
        write(QStringLiteral("bank/a.wav"), "RIFF");
        FixedCharsetSelector selector(QStringLiteral("UTF-8"));
        DiagnosticList diagnostics;
        const auto bank = VoiceBank::open(root() / "bank", &selector, diagnostics);
        if (!bank) {
            return std::nullopt;
        }
        Note note;
        note.lyric = QStringLiteral("a");
        note.noteNum = 60;
        note.length = 480;
        Project project;
        project.tracks.push_back({});
        project.tracks[0].notes.push_back(note);
        SynthPlan::Options options;
        options.cacheDirectory = root() / cacheName.toStdU16String();
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

        SynthToolLog log;
        const auto synthTools = somewhere();
        StubbedRunner runner(&log, synthTools, rendersTo(p->steps().at(0).cacheFile),
                             appends(p->outputFile()));

        DiagnosticList diagnostics;
        const auto outcome = runner.render(*p, synthTools, nullptr, diagnostics);

        QCOMPARE(log.resampled(), 1);
        QCOMPARE(log.appended(), 1);
        QVERIFY(outcome.rendered);
        QVERIFY(std::filesystem::exists(p->outputFile()));
        QVERIFY(!std::filesystem::exists(withSuffix(p->outputFile(), ".whd")));
        QVERIFY(!std::filesystem::exists(withSuffix(p->outputFile(), ".dat")));
        QCOMPARE(sizeOf(p->outputFile()), 44 + 100);
    }

    void a_wavtool_that_writes_the_track_directly_is_accepted() {
        const auto p = plan();
        QVERIFY(p.has_value());

        SynthToolLog log;
        const auto synthTools = somewhere();
        StubbedRunner runner(&log, synthTools, rendersTo(p->steps().at(0).cacheFile),
                             writesTrack(p->outputFile()));

        DiagnosticList diagnostics;
        const auto outcome = runner.render(*p, synthTools, nullptr, diagnostics);

        QVERIFY(outcome.rendered);
        QVERIFY(!hasError(diagnostics));
        QCOMPARE(sizeOf(p->outputFile()), qint64(directTrack.size()));
        QVERIFY(!std::filesystem::exists(withSuffix(p->outputFile(), ".whd")));
        QVERIFY(!std::filesystem::exists(withSuffix(p->outputFile(), ".dat")));
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

        SynthToolLog log;
        const auto synthTools = somewhere();
        StubbedRunner runner(&log, synthTools, rendersTo(p->steps().at(0).cacheFile),
                             appends(p->outputFile()));

        DiagnosticList diagnostics;
        QVERIFY(runner.render(*p, synthTools, nullptr, diagnostics).rendered);
        QCOMPARE(sizeOf(p->outputFile()), 44 + 100);
    }

    // The complete cycle: rendering the same project again, with every fragment reused,
    // produces the same track rather than a longer one.
    void a_second_render_comes_out_the_same_length() {
        const auto p = plan();
        QVERIFY(p.has_value());

        SynthToolLog log;
        const auto synthTools = somewhere();
        StubbedRunner runner(&log, synthTools, rendersTo(p->steps().at(0).cacheFile),
                             appends(p->outputFile()));

        DiagnosticList diagnostics;
        QVERIFY(runner.render(*p, synthTools, nullptr, diagnostics).rendered);
        const auto first = sizeOf(p->outputFile());

        QVERIFY(runner.render(*p, synthTools, nullptr, diagnostics).rendered);
        QCOMPARE(sizeOf(p->outputFile()), first);

        // The second render did not render the note again, because the note is unchanged.
        QCOMPARE(log.resampled(), 1);
        QCOMPARE(log.appended(), 2);
    }

    // A synth tool that starts, reports nothing and writes nothing is the most common render
    // failure, caused for example by an unreadable sample or an unknown flag. Exit codes are
    // unreliable, so success is determined by the existence of the fragment.
    void a_note_whose_piece_never_appeared_is_reported() {
        const auto p = plan();
        QVERIFY(p.has_value());

        SynthToolLog log;
        const auto synthTools = somewhere();
        StubbedRunner runner(&log, synthTools, writesNothing(), appends(p->outputFile()));

        DiagnosticList diagnostics;
        const auto outcome = runner.render(*p, synthTools, nullptr, diagnostics);

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

        SynthToolLog log;
        const auto synthTools = somewhere();
        StubbedRunner runner(&log, synthTools, writesNothing(), appends(p->outputFile()));
        runner.stopOnFirstFailure = true;

        DiagnosticList diagnostics;
        const auto outcome = runner.render(*p, synthTools, nullptr, diagnostics);

        QCOMPARE(outcome.failed, 1);
        QCOMPARE(log.appended(), 0);
        QVERIFY(!outcome.rendered);
        QVERIFY(!std::filesystem::exists(p->outputFile()));
    }

    // Reuse is the only part of rendering observable without a synth tool, because a reused note
    // starts no synth tool. The synth tool paths here are nonexistent, so a note reported as reused
    // cannot have been rendered.
    void a_piece_already_there_is_not_rendered_again() {
        const auto p = plan();
        QVERIFY(p.has_value());
        QCOMPARE(p->steps().size(), 1);

        QVERIFY(QDir().mkpath(QString::fromStdU16String(p->cacheDirectory().u16string())));
        write(QStringLiteral("cache/") +
                  QString::fromStdU16String(p->steps().at(0).cacheFile.filename().u16string()),
              "RIFF already rendered");

        SynthTools synthTools;
        synthTools.resampler = root() / "nowhere" / "resampler.exe";
        synthTools.wavtool = root() / "nowhere" / "wavtool.exe";

        DiagnosticList diagnostics;
        const ThreadedSynthRunner runner;
        const auto outcome = runner.render(*p, synthTools, nullptr, diagnostics);

        QCOMPARE(outcome.reused, 1);
        QCOMPARE(outcome.resampled, 0);
        QCOMPARE(outcome.failed, 0);
    }

    // Each note counts as two steps, its resampling and its append. A render from a full cache
    // runs only the wavtool, and its progress must advance with each append rather than remain
    // indeterminate until the end.
    void progress_advances_through_the_appends() {
        const auto p = plan(3);
        QVERIFY(p.has_value());
        fillCache(*p);

        SynthToolLog log;
        const auto synthTools = somewhere();
        StubbedRunner runner(&log, synthTools, writesNothing(), appends(p->outputFile()));
        Recorder recorder;

        DiagnosticList diagnostics;
        QVERIFY(runner.render(*p, synthTools, &recorder, diagnostics).rendered);

        QCOMPARE(log.resampled(), 0);
        const QList<std::pair<int, int>> expected{
            {3, 6},
            {4, 6},
            {5, 6},
            {6, 6}
        };
        QCOMPARE(recorder.reports, expected);
    }

    // A render from a full cache consists of appends only. A cancellation therefore takes effect
    // between the appends as well. The partial header and data are removed, and no track is
    // written.
    void a_render_is_cancelled_between_the_appends() {
        const auto p = plan(3);
        QVERIFY(p.has_value());
        fillCache(*p);

        SynthToolLog log;
        const auto synthTools = somewhere();
        StubbedRunner runner(&log, synthTools, writesNothing(), appends(p->outputFile()));
        Recorder recorder;
        recorder.cancelAt = 4;

        DiagnosticList diagnostics;
        const auto outcome = runner.render(*p, synthTools, &recorder, diagnostics);

        QVERIFY(outcome.cancelled);
        QVERIFY(!outcome.rendered);
        QCOMPARE(log.appended(), 1);
        QVERIFY(!std::filesystem::exists(p->outputFile()));
        QVERIFY(!std::filesystem::exists(withSuffix(p->outputFile(), ".whd")));
        QVERIFY(!std::filesystem::exists(withSuffix(p->outputFile(), ".dat")));
    }

    // The override for the case in which the synth tool itself has changed.
    void turning_reuse_off_renders_it_again() {
        const auto p = plan();
        QVERIFY(p.has_value());

        QVERIFY(QDir().mkpath(QString::fromStdU16String(p->cacheDirectory().u16string())));
        write(QStringLiteral("cache/") +
                  QString::fromStdU16String(p->steps().at(0).cacheFile.filename().u16string()),
              "RIFF already rendered");

        SynthTools synthTools;
        synthTools.resampler = root() / "nowhere" / "resampler.exe";
        synthTools.wavtool = root() / "nowhere" / "wavtool.exe";

        DiagnosticList diagnostics;
        ThreadedSynthRunner runner;
        runner.reuseCache = false;
        const auto outcome = runner.render(*p, synthTools, nullptr, diagnostics);

        QCOMPARE(outcome.reused, 0);
    }

    // The cache directory belongs to the project and persists with it, so fragments rendered
    // for a note before an edit must be removed. Fragments of notes outside this render remain.
    void fragments_no_longer_used_by_a_note_are_removed() {
        const auto p = plan();
        QVERIFY(p.has_value());

        const QString cache = QStringLiteral("cache/");
        const QString wanted =
            QString::fromStdU16String(p->steps().at(0).cacheFile.filename().u16string());
        write(cache + QStringLiteral("0_a_C4_stale0.wav"), "an older take of this note");
        write(cache + QStringLiteral("7_a_C4_stays0.wav"), "a note this render is not touching");
        write(cache + QStringLiteral("notes.txt"), "not a piece at all");
        write(cache + wanted, "RIFF already rendered");

        SynthTools synthTools;
        synthTools.resampler = root() / "nowhere" / "resampler.exe";
        synthTools.wavtool = root() / "nowhere" / "wavtool.exe";

        DiagnosticList diagnostics;
        const ThreadedSynthRunner runner;
        runner.render(*p, synthTools, nullptr, diagnostics);

        const auto there = [&](const QString &name) {
            return std::filesystem::exists(p->cacheDirectory() /
                                           std::filesystem::u8path(name.toStdString()));
        };
        QVERIFY(!there(QStringLiteral("0_a_C4_stale0.wav")));
        QVERIFY(there(QStringLiteral("7_a_C4_stays0.wav")));
        QVERIFY(there(QStringLiteral("notes.txt")));
        QVERIFY(there(wanted));
    }

    // A synth tool missing from its configured location must be reported. Rendering nothing
    // without a report is a failure that is costly for the user to diagnose.
    void synth_tools_that_are_not_there_are_reported() {
        const auto p = plan();
        QVERIFY(p.has_value());

        SynthTools synthTools;
        synthTools.resampler = root() / "nowhere" / "resampler.exe";
        synthTools.wavtool = root() / "nowhere" / "wavtool.exe";

        DiagnosticList diagnostics;
        const ThreadedSynthRunner runner;
        const auto outcome = runner.render(*p, synthTools, nullptr, diagnostics);

        QVERIFY(!outcome.rendered);
        QCOMPARE(outcome.resampled, 0);
        QCOMPARE(outcome.failed, 1);
        QVERIFY(hasError(diagnostics));
        QVERIFY(!std::filesystem::exists(p->outputFile()));
    }

    // The synth tools write into the directory, so it must exist before they run. They do not
    // create it, and their diagnostic for a nonexistent path is synth tool-specific.
    void the_cache_folder_is_created() {
        const auto p = plan();
        QVERIFY(p.has_value());
        QVERIFY(!std::filesystem::exists(p->cacheDirectory()));

        SynthTools synthTools;
        synthTools.resampler = root() / "nowhere" / "resampler.exe";
        synthTools.wavtool = root() / "nowhere" / "wavtool.exe";

        DiagnosticList diagnostics;
        const ThreadedSynthRunner runner;
        runner.render(*p, synthTools, nullptr, diagnostics);

        QVERIFY(std::filesystem::is_directory(p->cacheDirectory()));
    }

    // A resampler killed by a cancellation may have written part of its fragment, which the next
    // render would otherwise reuse as complete.
    void a_cancelled_resampler_call_leaves_no_fragment() {
        const auto p = plan(2);
        QVERIFY(p.has_value());
        const auto synthTools = somewhere();
        std::atomic_int calls{0};
        PollingRunner runner([&](const std::filesystem::path &program, const QStringList &arguments,
                                 const std::function<bool()> &cancelled) {
            SynthToolRun run;
            run.started = true;
            if (program == synthTools.resampler) {
                ++calls;
                std::ofstream(fragmentOf(arguments), std::ios::binary) << "RIFF half";
                run.cancelled = waitForCancellation(cancelled);
            }
            return run;
        });
        Recorder recorder;
        recorder.cancelAt = 0;

        DiagnosticList diagnostics;
        const auto outcome = runner.render(*p, synthTools, &recorder, diagnostics);

        QVERIFY(outcome.cancelled);
        QVERIFY(calls.load() >= 1);
        for (const auto &step : p->steps()) {
            QVERIFY(!std::filesystem::exists(step.cacheFile));
        }
    }

    // A resampler killed at its time limit counts as failed, and its fragment is removed.
    void a_timed_out_resampler_call_leaves_no_fragment_and_fails() {
        const auto p = plan(2);
        QVERIFY(p.has_value());
        const auto synthTools = somewhere();
        PollingRunner runner([&](const std::filesystem::path &program, const QStringList &arguments,
                                 const std::function<bool()> &) {
            SynthToolRun run;
            run.started = true;
            if (program == synthTools.resampler) {
                std::ofstream(fragmentOf(arguments), std::ios::binary) << "RIFF half";
                run.timedOut = true;
            }
            return run;
        });

        DiagnosticList diagnostics;
        const auto outcome = runner.render(*p, synthTools, nullptr, diagnostics);

        QVERIFY(!outcome.cancelled);
        QCOMPARE(outcome.failed, 2);
        for (const auto &step : p->steps()) {
            QVERIFY(!std::filesystem::exists(step.cacheFile));
        }
    }

    // A wavtool cancelled in the middle of the track leaves neither of its two files.
    void a_cancelled_wavtool_call_removes_its_two_files() {
        const auto p = plan(3);
        QVERIFY(p.has_value());
        const auto synthTools = somewhere();
        const auto track = p->outputFile();
        std::atomic_int appends{0};
        PollingRunner runner([&](const std::filesystem::path &program, const QStringList &arguments,
                                 const std::function<bool()> &) {
            SynthToolRun run;
            run.started = true;
            if (program == synthTools.resampler) {
                std::ofstream(fragmentOf(arguments), std::ios::binary) << "RIFF piece";
            } else if (appends++ == 0) {
                std::ofstream(withSuffix(track, ".whd"), std::ios::binary) << std::string(44, 'H');
                std::ofstream(withSuffix(track, ".dat"), std::ios::binary) << std::string(100, 'D');
            } else {
                run.cancelled = true;
            }
            return run;
        });

        DiagnosticList diagnostics;
        const auto outcome = runner.render(*p, synthTools, nullptr, diagnostics);

        QVERIFY(outcome.cancelled);
        QVERIFY(!outcome.rendered);
        QCOMPARE(appends.load(), 2);
        QVERIFY(!std::filesystem::exists(withSuffix(track, ".whd")));
        QVERIFY(!std::filesystem::exists(withSuffix(track, ".dat")));
    }

    // A failure under stopOnFirstFailure starts no further notes, but the calls that run already
    // are not killed, unlike those of a cancellation.
    void a_failure_does_not_kill_the_running_calls() {
        const auto p = plan(3);
        QVERIFY(p.has_value());
        const auto synthTools = somewhere();
        std::atomic_int calls{0};
        std::atomic_bool killed{false};
        PollingRunner runner([&](const std::filesystem::path &program, const QStringList &arguments,
                                 const std::function<bool()> &cancelled) {
            SynthToolRun run;
            run.started = true;
            if (program != synthTools.resampler) {
                return run;
            }
            if (calls++ == 0) {
                // Fails after the other calls have started
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return run;
            }
            for (int i = 0; i < 30; ++i) {
                if (cancelled()) {
                    killed = true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            std::ofstream(fragmentOf(arguments), std::ios::binary) << "RIFF piece";
            return run;
        });
        runner.stopOnFirstFailure = true;
        runner.threadCount = 3;

        DiagnosticList diagnostics;
        const auto outcome = runner.render(*p, synthTools, nullptr, diagnostics);

        QCOMPARE(calls.load(), 3);
        QVERIFY(!killed.load());
        QVERIFY(!outcome.cancelled);
        QCOMPARE(outcome.failed, 1);
        int fragments = 0;
        for (const auto &step : p->steps()) {
            fragments += std::filesystem::exists(step.cacheFile) ? 1 : 0;
        }
        QCOMPARE(fragments, 2);
    }

    // A plan that the classic runner refuses for its scripts is refused here as well, before any
    // synth tool runs or any file is written.
    void a_plan_the_scripts_cannot_carry_is_refused_as_in_the_classic_runner() {
        const TextCodec codec;
        if (codec.isUtf8()) {
            QSKIP("The system code page is UTF-8, which represents every character.");
        }
        const auto outside = QString::fromUtf8("\xF0\xA0\xAE\xB7");
        if (codec.canEncode(outside)) {
            QSKIP("The system code page represents the character.");
        }
        const auto p = planWithCache(QStringLiteral("cache") + outside);
        QVERIFY(p.has_value());
        DiagnosticList classic;
        QVERIFY(!ClassicSynthRunner().scriptFiles(root() / "scripts", *p, somewhere(), classic));

        SynthToolLog log;
        const auto synthTools = somewhere();
        StubbedRunner runner(&log, synthTools, rendersTo(p->steps().at(0).cacheFile),
                             appends(p->outputFile()));
        DiagnosticList diagnostics;
        const auto outcome = runner.render(*p, synthTools, nullptr, diagnostics);
        QVERIFY(!outcome.rendered);
        QVERIFY(hasError(diagnostics));
        QCOMPARE(log.resampled(), 0);
        QVERIFY(!std::filesystem::exists(p->cacheDirectory()));
    }

    // With a script directory, the scripts that the classic runner would write are written
    // there, for the synth tools that read them.
    void the_scripts_of_the_classic_runner_are_written_to_the_script_directory() {
        const auto p = plan();
        QVERIFY(p.has_value());
        const auto directory = root() / "scripts";
        DiagnosticList classic;
        const auto expected = ClassicSynthRunner().scriptFiles(directory, *p, somewhere(), classic);
        QVERIFY(expected.has_value());

        SynthToolLog log;
        const auto synthTools = somewhere();
        StubbedRunner runner(&log, synthTools, rendersTo(p->steps().at(0).cacheFile),
                             appends(p->outputFile()));
        runner.scriptDirectory = directory;
        DiagnosticList diagnostics;
        QVERIFY(runner.render(*p, synthTools, nullptr, diagnostics).rendered);
        for (const auto &[path, bytes] : *expected) {
            QFile file(QString::fromStdU16String(path.u16string()));
            QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.fileName()));
            QCOMPARE(file.readAll(), bytes);
        }
    }
};

QTEST_APPLESS_MAIN(test_ThreadedSynthRunner)

#include "test_ThreadedSynthRunner.moc"
