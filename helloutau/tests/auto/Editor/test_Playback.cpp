#include <atomic>
#include <filesystem>
#include <fstream>
#include <future>

#include <QtCore/QTemporaryDir>
#include <QtCore/QThread>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <hellokit/Edit/ProjectDocument.h>
#include <hellokit/Edit/ProjectRefs.h>
#include <hellokit/Synth/SynthPlan.h>

#include <helloutau/Audio/AudioEngine.h>

#include <helloutau/Editor/Playback.h>

using namespace hello;
using namespace hello::daw;
namespace fs = std::filesystem;

namespace {

    void writeBytes(const fs::path &path, const QByteArray &bytes) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(bytes.constData(), bytes.size());
    }

    // \a frames of 16-bit mono silence at 44100 Hz, a tenth of a second by default
    QByteArray silence(quint32 frames = 4410) {
        const quint32 data = frames * 2;
        QByteArray bytes("RIFF");
        const auto u32 = [&bytes](quint32 value) {
            for (int i = 0; i < 4; ++i) {
                bytes.append(char((value >> (8 * i)) & 0xff));
            }
        };
        const auto u16 = [&bytes](quint16 value) {
            bytes.append(char(value & 0xff));
            bytes.append(char(value >> 8));
        };
        u32(36 + data);
        bytes.append("WAVEfmt ");
        u32(16);
        u16(1);
        u16(1);
        u32(44100);
        u32(44100 * 2);
        u16(2);
        u16(16);
        bytes.append("data");
        u32(data);
        bytes.append(QByteArray(int(data), '\0'));
        return bytes;
    }

    // Writes silence as the track file instead of running synth tools, and records the plans.
    class SilentRunner : public kit::SynthRunner {
    public:
        mutable QList<int> stepCounts;
        mutable std::atomic<int> started = 0;
        // The number of renders that have returned
        mutable std::atomic<int> ended = 0;
        mutable QList<fs::path> caches;
        mutable std::atomic<bool> waitForCancel = false;
        // Runs on until released, cancelled or not, as a script does
        mutable std::atomic<bool> hold = false;
        // The frames of the track file
        quint32 frames = 4410;

        kit::SynthOutcome render(const kit::SynthPlan &plan, const kit::SynthTools &synthTools,
                                 kit::SynthObserver *observer,
                                 kit::DiagnosticList &diagnostics) const override {
            Q_UNUSED(synthTools);
            Q_UNUSED(diagnostics);
            caches.push_back(plan.cacheDirectory());
            stepCounts.push_back(int(plan.steps().size()));
            ++started;
            kit::SynthOutcome outcome;
            while (hold.load()) {
                QThread::msleep(5);
            }
            while (waitForCancel.load()) {
                if (observer->cancelled()) {
                    outcome.cancelled = true;
                    ++ended;
                    return outcome;
                }
                QThread::msleep(5);
            }
            observer->progressed(int(plan.steps().size()), int(plan.steps().size()));
            writeBytes(plan.outputFile(), silence(frames));
            outcome.rendered = true;
            ++ended;
            return outcome;
        }
    };

    // A document with two notes, saved beside a voice bank that sings them
    std::unique_ptr<kit::ProjectDocument> singingDocument(const QTemporaryDir &dir) {
        const auto root = fs::path(dir.path().toStdU16String());
        fs::create_directories(root / "bank");
        writeBytes(root / "bank" / "oto.ini", "#Charset:UTF-8\r\na.wav=a,0,0,0,0,0\r\n");
        writeBytes(root / "bank" / "a.wav", {});

        kit::Note a;
        a.lyric = QStringLiteral("a");
        a.length = 480;
        a.noteNum = 60;
        kit::Track track;
        track.voiceDir = QString::fromStdU16String((root / "bank").u16string());
        track.notes = {a, a};
        kit::Project project;
        project.tracks.push_back(track);
        kit::DiagnosticList diagnostics;
        project.save(root / "song.usth", diagnostics);

        auto document = kit::ProjectDocument::open(root / "song.usth", nullptr, diagnostics);
        if (document) {
            document->loadVoiceBank({}, nullptr, diagnostics);
        }
        return document;
    }

    // Puts the fragment of every note of \a document in the cache of \a playback, so that no
    // resampler needs to run. The fragments are named for \a resampler, as a render with it
    // names them.
    bool writeFragments(Playback &playback, const kit::ProjectDocument &document,
                        const fs::path &resampler) {
        kit::SynthPlan::Options options;
        options.cacheDirectory = *playback.cacheDirectoryFor(document);
        options.resampler = resampler;
        options.outputFile = options.cacheDirectory / "playback.wav";
        kit::DiagnosticList diagnostics;
        const auto plan = kit::SynthPlan::make(document.session()->snapshot(),
                                               *document.voiceBank(), options, diagnostics);
        if (!plan) {
            return false;
        }
        fs::create_directories(options.cacheDirectory);
        for (const auto &step : plan->steps()) {
            writeBytes(step.cacheFile, silence());
        }
        return true;
    }

    // The temporary directory of a playback inside \a dir, as the project window supplies one
    fs::path temporaryOf(const QTemporaryDir &dir) {
        const auto path = fs::path(dir.path().toStdU16String()) / "temporary";
        fs::create_directories(path);
        return path;
    }

    kit::SynthTools someSynthTools() {
        kit::SynthTools synthTools;
        synthTools.resampler = "resampler.exe";
        synthTools.wavtool = "wavtool.exe";
        return synthTools;
    }

}

class test_Playback : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Beside the .usth, beside the UST it came from, or else in the temporary directory, and
    // nowhere without one.
    void the_cache_is_where_utau_keeps_it() {
        QTemporaryDir dir;
        Playback playback(nullptr, temporaryOf(dir), nullptr);

        kit::ProjectDocument unsaved;
        QCOMPARE(playback.cacheDirectoryFor(unsaved),
                 std::optional<fs::path>(temporaryOf(dir) / "cache"));
        Playback without;
        QVERIFY(!without.cacheDirectoryFor(unsaved));

        const auto document = singingDocument(dir);
        QVERIFY(document);
        QCOMPARE(*playback.cacheDirectoryFor(*document),
                 fs::path(dir.path().toStdU16String()) / "song.cache");

        // A UST that states its encoding opens without asking.
        const auto ust = fs::path(dir.path().toStdU16String()) / "imported.ust";
        writeBytes(ust, "[#VERSION]\r\nUST Version1.2\r\nCharset=UTF-8\r\n[#SETTING]\r\n"
                        "Tempo=120.00\r\nTracks=1\r\nMode2=True\r\n[#0000]\r\nLength=480\r\n"
                        "Lyric=a\r\nNoteNum=60\r\n[#TRACKEND]\r\n");
        kit::DiagnosticList diagnostics;
        const auto imported = kit::ProjectDocument::open(ust, nullptr, diagnostics);
        QVERIFY(imported);
        QVERIFY(imported->filePath().empty());
        QCOMPARE(*playback.cacheDirectoryFor(*imported),
                 fs::path(dir.path().toStdU16String()) / "imported.cache");
    }

    // Playing requires synth tools, a voice bank and the temporary directory.
    void playing_needs_synth_tools_and_a_voice_bank() {
        QTemporaryDir dir;
        Playback playback(nullptr, temporaryOf(dir), nullptr);
        kit::DiagnosticList diagnostics;

        const auto document = singingDocument(dir);
        QVERIFY(document);
        QVERIFY(!playback.play(*document, std::nullopt, {}, diagnostics));
        QVERIFY(kit::hasError(diagnostics));

        diagnostics.clear();
        Playback without;
        QVERIFY(!without.play(*document, std::nullopt, someSynthTools(), diagnostics));
        QVERIFY(!without.renderTrack(*document, temporaryOf(dir) / "out.wav", someSynthTools(),
                                     diagnostics));
        QVERIFY(!without.preview(*document, std::nullopt, someSynthTools(), diagnostics));
        QVERIFY(!without.prepare(*document, std::nullopt, someSynthTools(), diagnostics));
        QCOMPARE(diagnostics.size(), 4);

        diagnostics.clear();
        kit::ProjectDocument silent;
        QVERIFY(!playback.play(silent, std::nullopt, someSynthTools(), diagnostics));
        QVERIFY(kit::hasError(diagnostics));
        QCOMPARE(playback.state(), Playback::Stopped);
    }

    // The selection is rendered, into the cache of the document, then played to its end.
    void a_render_is_played_to_its_end() {
        if (AudioEngine::instance()->sampleRate() <= 0) {
            QSKIP("This machine has no audio output device.");
        }
        QTemporaryDir dir;
        const auto document = singingDocument(dir);
        QVERIFY(document);
        Playback playback(nullptr, temporaryOf(dir), nullptr);
        const auto runner = std::make_shared<SilentRunner>();
        playback.setRunner(runner);
        QSignalSpy states(&playback, &Playback::stateChanged);
        QSignalSpy progress(&playback, &Playback::progressed);
        QSignalSpy planProgress(&playback, &Playback::planProgressed);
        QSignalSpy failures(&playback, &Playback::failed);

        kit::DiagnosticList diagnostics;
        QVERIFY(playback.lastRenderFile().empty());
        QVERIFY(playback.play(*document, std::make_pair(1, 1), someSynthTools(), diagnostics));
        QCOMPARE(playback.state(), Playback::Rendering);
        QVERIFY(playback.lastRenderFile().empty());
        QTRY_COMPARE_WITH_TIMEOUT(states.size(), 3, 5000);
        // The track file of the render, which Save Last Played copies
        QCOMPARE(playback.lastRenderFile(), temporaryOf(dir) / "temp.wav");
        QCOMPARE(failures.size(), 0);
        QCOMPARE(states.at(1).at(0).value<Playback::State>(), Playback::Playing);
        QCOMPARE(states.at(2).at(0).value<Playback::State>(), Playback::Stopped);
        // The start of the render, of unknown steps, and its end
        QCOMPARE(progress.size(), 2);
        QCOMPARE(progress.first(), (QList<QVariant>{0, 0}));
        QCOMPARE(progress.last(), (QList<QVariant>{1, 1}));
        // The plan of the one note, made on the worker thread before the render
        QVERIFY(!planProgress.isEmpty());
        QCOMPARE(planProgress.first(), (QList<QVariant>{0, 1}));
        QCOMPARE(planProgress.last(), (QList<QVariant>{1, 1}));
        QCOMPARE(runner->stepCounts, QList<int>{1});
        QCOMPARE(runner->caches, QList<fs::path>{*playback.cacheDirectoryFor(*document)});
        QVERIFY(fs::is_regular_file(temporaryOf(dir) / "temp.wav"));
    }

    // The whole track is rendered into a file of the caller's choice, without playing it and
    // without replacing the render kept for playback.
    void a_track_is_rendered_into_a_file() {
        QTemporaryDir dir;
        const auto document = singingDocument(dir);
        QVERIFY(document);
        Playback playback(nullptr, temporaryOf(dir), nullptr);
        const auto runner = std::make_shared<SilentRunner>();
        playback.setRunner(runner);
        QSignalSpy rendered(&playback, &Playback::trackRendered);
        QSignalSpy failures(&playback, &Playback::failed);

        const auto file = fs::path(dir.path().toStdU16String()) / "out" / "song.wav";
        fs::create_directories(file.parent_path());
        kit::DiagnosticList diagnostics;
        QVERIFY(playback.renderTrack(*document, file, someSynthTools(), diagnostics));
        QCOMPARE(playback.state(), Playback::Rendering);
        QTRY_COMPARE_WITH_TIMEOUT(rendered.size(), 1, 5000);
        QCOMPARE(failures.size(), 0);
        QCOMPARE(rendered.first().first().value<fs::path>(), file);
        QCOMPARE(playback.state(), Playback::Stopped);
        QVERIFY(fs::is_regular_file(file));
        QVERIFY(playback.lastRenderFile().empty());
    }

    // While a render plays, the playhead moves with what the device plays, from where the track
    // file starts.
    void the_playhead_follows_a_render_as_it_plays() {
        if (AudioEngine::instance()->sampleRate() <= 0) {
            QSKIP("This machine has no audio output device.");
        }
        QTemporaryDir dir;
        const auto document = singingDocument(dir);
        QVERIFY(document);
        Playback playback(nullptr, temporaryOf(dir), nullptr);
        const auto runner = std::make_shared<SilentRunner>();
        runner->frames = 44100;
        playback.setRunner(runner);

        kit::DiagnosticList diagnostics;
        QVERIFY(playback.play(*document, std::nullopt, someSynthTools(), diagnostics));
        QTRY_COMPARE_WITH_TIMEOUT(playback.state(), Playback::Playing, 5000);
        const auto first = playback.position();
        QVERIFY(first);
        QTest::qWait(400);
        const auto later = playback.position();
        QVERIFY(later);
        // Within what a device may hold back, and a timer may be late
        const double moved = *later - *first;
        QVERIFY2(moved > 250 && moved < 600, qPrintable(QString::number(moved)));
        playback.stop();
    }

    // A render plays again without the synth tools while its notes stay the same, and is rendered
    // anew once they change.
    void a_render_plays_again_while_its_notes_stay() {
        if (AudioEngine::instance()->sampleRate() <= 0) {
            QSKIP("This machine has no audio output device.");
        }
        QTemporaryDir dir;
        const auto document = singingDocument(dir);
        QVERIFY(document);
        Playback playback(nullptr, temporaryOf(dir), nullptr);
        const auto runner = std::make_shared<SilentRunner>();
        playback.setRunner(runner);

        kit::DiagnosticList diagnostics;
        QVERIFY(playback.play(*document, std::nullopt, someSynthTools(), diagnostics));
        QTRY_COMPARE_WITH_TIMEOUT(playback.state(), Playback::Playing, 5000);
        playback.stop();
        QVERIFY(playback.play(*document, std::nullopt, someSynthTools(), diagnostics));
        QTRY_COMPARE_WITH_TIMEOUT(playback.state(), Playback::Playing, 5000);
        QCOMPARE(runner->started.load(), 1);
        playback.stop();

        // Another range, or an edit, renders anew.
        QVERIFY(playback.play(*document, std::make_pair(1, 1), someSynthTools(), diagnostics));
        QCOMPARE(playback.state(), Playback::Rendering);
        QTRY_COMPARE_WITH_TIMEOUT(playback.state(), Playback::Playing, 5000);
        playback.stop();
        {
            const auto session = document->session();
            auto tx = session->transaction(QStringLiteral("transpose"));
            kit::ProjectRef(session).tracks().at(0).notes().at(1).setNoteNum(62);
            tx.commit();
        }
        QVERIFY(playback.play(*document, std::make_pair(1, 1), someSynthTools(), diagnostics));
        QCOMPARE(playback.state(), Playback::Rendering);
        QTRY_COMPARE_WITH_TIMEOUT(runner->started.load(), 3, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(playback.state(), Playback::Playing, 5000);
        playback.stop();

        // So does a sample recorded again, which names its fragment anew.
        writeBytes(fs::path(dir.path().toStdU16String()) / "bank" / "a.wav", "RIFF");
        QVERIFY(document->loadVoiceBank({}, nullptr, diagnostics));
        QVERIFY(playback.play(*document, std::make_pair(1, 1), someSynthTools(), diagnostics));
        QCOMPARE(playback.state(), Playback::Rendering);
        playback.stop();
    }

    // A paused render keeps where it was, and goes on from there.
    void a_render_pauses_and_goes_on() {
        if (AudioEngine::instance()->sampleRate() <= 0) {
            QSKIP("This machine has no audio output device.");
        }
        QTemporaryDir dir;
        const auto document = singingDocument(dir);
        QVERIFY(document);
        Playback playback(nullptr, temporaryOf(dir), nullptr);
        const auto runner = std::make_shared<SilentRunner>();
        runner->frames = 44100;
        playback.setRunner(runner);
        QVERIFY(!playback.pause());

        kit::DiagnosticList diagnostics;
        QVERIFY(playback.play(*document, std::nullopt, someSynthTools(), diagnostics));
        QTRY_COMPARE_WITH_TIMEOUT(playback.state(), Playback::Playing, 5000);
        QTest::qWait(300);
        QSignalSpy states(&playback, &Playback::stateChanged);
        QVERIFY(playback.pause());
        QCOMPARE(playback.state(), Playback::Paused);
        QCOMPARE(states.size(), 1);
        QVERIFY(!playback.isPreviewPaused());
        const auto paused = playback.position();
        QVERIFY(paused);
        QTest::qWait(200);
        QCOMPARE(playback.position(), paused);

        QVERIFY(playback.resume());
        QCOMPARE(playback.state(), Playback::Playing);
        QTest::qWait(200);
        const auto later = playback.position();
        QVERIFY(later);
        QVERIFY2(*later > *paused + 100 && *later < *paused + 400,
                 qPrintable(QStringLiteral("%1 %2").arg(*paused).arg(*later)));

        playback.pause();
        playback.stop();
        QCOMPARE(playback.state(), Playback::Stopped);
        QVERIFY(!playback.position());
        QVERIFY(!playback.resume());
    }

    // A preview plays from the time asked for to the end, here within the second note, reading
    // the fragments in the cache rather than running the resampler, which here does not exist.
    void a_preview_plays_from_a_time_to_the_end() {
        if (AudioEngine::instance()->sampleRate() <= 0) {
            QSKIP("This machine has no audio output device.");
        }
        QTemporaryDir dir;
        const auto document = singingDocument(dir);
        QVERIFY(document);
        Playback playback(nullptr, temporaryOf(dir), nullptr);

        QVERIFY(writeFragments(playback, *document,
                               fs::path(dir.path().toStdU16String()) / "missing.exe"));

        kit::DiagnosticList diagnostics;
        QSignalSpy states(&playback, &Playback::stateChanged);
        kit::SynthTools synthTools;
        synthTools.resampler = fs::path(dir.path().toStdU16String()) / "missing.exe";
        synthTools.wavtool = fs::path(dir.path().toStdU16String()) / "missing-wavtool.exe";
        QVERIFY(playback.preview(*document, 750.0, synthTools, diagnostics));
        // Rendering until the plan of the track is made
        QCOMPARE(playback.state(), Playback::Rendering);
        QVERIFY(playback.planProgress());
        QTRY_COMPARE_WITH_TIMEOUT(playback.state(), Playback::Playing, 5000);
        QVERIFY(!playback.planProgress());
        const auto first = playback.position();
        QVERIFY(first);
        // From 750 ms on, half way through the second note at 120 bpm, and no more ahead than a
        // device that pulls a buffer as it starts has read
        QVERIFY2(*first >= 750 && *first < 1050, qPrintable(QString::number(*first)));

        QTRY_COMPARE_WITH_TIMEOUT(playback.state(), Playback::Stopped, 5000);
        QCOMPARE(states.size(), 3);
        QCOMPARE(playback.pendingNotes(), 0);
        QVERIFY(playback.takePreviewDiagnostics().isEmpty());
    }

    void a_preview_needs_a_resampler() {
        QTemporaryDir dir;
        const auto document = singingDocument(dir);
        QVERIFY(document);
        Playback playback(nullptr, temporaryOf(dir), nullptr);
        kit::DiagnosticList diagnostics;
        QVERIFY(!playback.preview(*document, std::nullopt, {}, diagnostics));
        QVERIFY(kit::hasError(diagnostics));
        QVERIFY(!playback.isBuffering());
        QCOMPARE(playback.pendingNotes(), 0);
    }

    // Stopping during a render cancels it, and nothing plays or fails afterwards.
    void stopping_cancels_the_render() {
        QTemporaryDir dir;
        const auto document = singingDocument(dir);
        QVERIFY(document);
        Playback playback(nullptr, temporaryOf(dir), nullptr);
        const auto runner = std::make_shared<SilentRunner>();
        runner->waitForCancel = true;
        playback.setRunner(runner);
        QSignalSpy states(&playback, &Playback::stateChanged);
        QSignalSpy failures(&playback, &Playback::failed);

        kit::DiagnosticList diagnostics;
        if (!playback.play(*document, std::nullopt, someSynthTools(), diagnostics)) {
            QSKIP("This machine has no audio output device.");
        }
        QTRY_COMPARE(runner->started.load(), 1);
        playback.stop();
        QCOMPARE(playback.state(), Playback::Stopped);
        QTest::qWait(100);
        QCOMPARE(states.size(), 2);
        QCOMPARE(failures.size(), 0);
        QVERIFY(!playback.position());
    }

    // A plan is made on the worker thread, so that a range without notes is reported by
    // failed(), and nothing is rendered.
    void a_failure_of_the_plan_is_reported_later() {
        QTemporaryDir dir;
        const auto document = singingDocument(dir);
        QVERIFY(document);
        Playback playback(nullptr, temporaryOf(dir), nullptr);
        const auto runner = std::make_shared<SilentRunner>();
        playback.setRunner(runner);
        QSignalSpy failures(&playback, &Playback::failed);

        kit::DiagnosticList diagnostics;
        if (!playback.play(*document, std::make_pair(5, 9), someSynthTools(), diagnostics)) {
            QSKIP("This machine has no audio output device.");
        }
        QVERIFY(diagnostics.isEmpty());
        QCOMPARE(playback.state(), Playback::Rendering);
        QTRY_COMPARE_WITH_TIMEOUT(failures.size(), 1, 5000);
        QVERIFY(kit::hasError(failures.first().first().value<kit::DiagnosticList>()));
        QCOMPARE(playback.state(), Playback::Stopped);
        QCOMPARE(runner->started.load(), 0);
    }

    // Clearing the cache deletes its files, not its folders, and the fragments in memory; not
    // while a render goes on, which writes into it.
    void the_cache_is_cleared_of_its_files() {
        using S = kit::RealtimeSynth;
        QTemporaryDir dir;
        const auto document = singingDocument(dir);
        QVERIFY(document);
        Playback playback(nullptr, temporaryOf(dir), nullptr);
        QVERIFY(writeFragments(playback, *document,
                               fs::path(dir.path().toStdU16String()) / "missing.exe"));
        const auto cache = *playback.cacheDirectoryFor(*document);
        fs::create_directories(cache / "kept");
        kit::SynthTools synthTools;
        synthTools.resampler = fs::path(dir.path().toStdU16String()) / "missing.exe";
        synthTools.wavtool = fs::path(dir.path().toStdU16String()) / "missing-wavtool.exe";
        kit::DiagnosticList diagnostics;
        QVERIFY(playback.prepare(*document, std::nullopt, synthTools, diagnostics));
        QTRY_COMPARE(playback.noteStates(), (QList<S::NoteState>{S::Ready, S::Ready}));

        QCOMPARE(playback.clearCache(*document, diagnostics), std::optional<int>(2));
        QVERIFY(diagnostics.isEmpty());
        QVERIFY(fs::is_directory(cache / "kept"));
        playback.refreshNoteStates(*document);
        QTRY_COMPARE(playback.noteStates(), (QList<S::NoteState>{S::Waiting, S::Waiting}));

        const auto runner = std::make_shared<SilentRunner>();
        runner->hold = true;
        playback.setRunner(runner);
        if (!playback.play(*document, std::nullopt, someSynthTools(), diagnostics)) {
            return;
        }
        QTRY_COMPARE(runner->started.load(), 1);
        QVERIFY(!playback.clearCache(*document, diagnostics));
        QVERIFY(kit::hasError(diagnostics));
        runner->hold = false;
        playback.stop();
    }

    // A render that goes on after it was cancelled, as a script does in its console, has to
    // end before the next can start, which would write the same files.
    void a_render_cancelled_ends_before_the_next() {
        QTemporaryDir dir;
        const auto document = singingDocument(dir);
        QVERIFY(document);
        Playback playback(nullptr, temporaryOf(dir), nullptr);
        const auto runner = std::make_shared<SilentRunner>();
        runner->hold = true;
        playback.setRunner(runner);

        kit::DiagnosticList diagnostics;
        if (!playback.play(*document, std::nullopt, someSynthTools(), diagnostics)) {
            QSKIP("This machine has no audio output device.");
        }
        QTRY_COMPARE(runner->started.load(), 1);
        playback.stop();
        QVERIFY(!playback.play(*document, std::nullopt, someSynthTools(), diagnostics));
        QVERIFY(kit::hasError(diagnostics));
        QCOMPARE(runner->started.load(), 1);

        runner->hold = false;
        bool started = false;
        for (int attempts = 0; attempts < 500 && !started; ++attempts) {
            diagnostics.clear();
            started = playback.play(*document, std::nullopt, someSynthTools(), diagnostics);
            if (!started) {
                QTest::qWait(10);
            }
        }
        QVERIFY2(started, qPrintable(diagnostics.value(0).message));
        QTRY_COMPARE(playback.state(), Playback::Playing);
    }

    // The track is rendered in the background, here from the cache, without playing, and
    // release() forgets it.
    void the_track_is_rendered_in_the_background() {
        QTemporaryDir dir;
        const auto document = singingDocument(dir);
        QVERIFY(document);
        Playback playback(nullptr, temporaryOf(dir), nullptr);
        kit::DiagnosticList diagnostics;
        QVERIFY(!playback.prepare(*document, std::nullopt, {}, diagnostics));
        QVERIFY(kit::hasError(diagnostics));

        // Without the synth, by the cache scanned on a worker thread: nothing there yet, then
        // the fragments of the resampler of the synth tools set
        using S = kit::RealtimeSynth;
        kit::SynthTools synthTools;
        synthTools.resampler = fs::path(dir.path().toStdU16String()) / "missing.exe";
        synthTools.wavtool = fs::path(dir.path().toStdU16String()) / "missing-wavtool.exe";
        playback.setSynthTools(synthTools);
        QSignalSpy changes(&playback, &Playback::noteStatesChanged);
        QVERIFY(playback.noteStates().isEmpty());
        playback.refreshNoteStates(*document);
        QVERIFY(playback.noteStates().isEmpty());
        QTRY_COMPARE(changes.size(), 1);
        QCOMPARE(playback.noteStates(), (QList<S::NoteState>{S::Waiting, S::Waiting}));
        QVERIFY(writeFragments(playback, *document,
                               fs::path(dir.path().toStdU16String()) / "missing.exe"));
        playback.refreshNoteStates(*document);
        QTRY_COMPARE(playback.noteStates(), (QList<S::NoteState>{S::Ready, S::Ready}));
        QSignalSpy states(&playback, &Playback::stateChanged);
        diagnostics.clear();
        QVERIFY(playback.prepare(*document, 750.0, synthTools, diagnostics));
        // The plan is made on a worker thread, and the synth takes the fragments from the cache.
        QVERIFY(playback.planProgress());
        QTRY_VERIFY(!playback.planProgress());
        QTRY_COMPARE(playback.noteStates(), (QList<S::NoteState>{S::Ready, S::Ready}));
        QCOMPARE(playback.pendingNotes(), 0);
        QVERIFY(playback.takePreviewDiagnostics().isEmpty());
        QCOMPARE(playback.state(), Playback::Stopped);
        QCOMPARE(states.size(), 0);

        // An edit renders the note anew, whose fragment the resampler cannot make.
        {
            const auto session = document->session();
            auto tx = session->transaction(QStringLiteral("transpose"));
            kit::ProjectRef(session).tracks().at(0).notes().at(1).setNoteNum(62);
            tx.commit();
        }
        playback.updatePlan(*document);
        kit::DiagnosticList failed;
        QTRY_VERIFY((failed.append(playback.takePreviewDiagnostics()), !failed.isEmpty()));
        QCOMPARE(failed.last().severity, kit::DiagnosticSeverity::Warning);
        QVERIFY(failed.last().noteIndex == 1);
        // As the synth has them
        QCOMPARE(playback.noteStates(), (QList<S::NoteState>{S::Ready, S::Failed}));

        // Released, nothing is rendered after an edit.
        playback.release();
        QCOMPARE(playback.pendingNotes(), 0);
        {
            const auto session = document->session();
            auto tx = session->transaction(QStringLiteral("transpose"));
            kit::ProjectRef(session).tracks().at(0).notes().at(1).setNoteNum(64);
            tx.commit();
        }
        playback.updatePlan(*document);
        QTest::qWait(200);
        QVERIFY(playback.takePreviewDiagnostics().isEmpty());
    }

    // stopAndWait() returns after the render has ended, also a render that runs on after it was
    // cancelled, so that the temporary directory can be replaced afterwards.
    void stop_and_wait_waits_for_the_render() {
        QTemporaryDir dir;
        const auto document = singingDocument(dir);
        QVERIFY(document);
        Playback playback(nullptr, temporaryOf(dir), nullptr);
        const auto runner = std::make_shared<SilentRunner>();
        runner->hold = true;
        playback.setRunner(runner);
        kit::DiagnosticList diagnostics;
        if (!playback.play(*document, std::nullopt, someSynthTools(), diagnostics)) {
            QSKIP("This machine has no audio output device.");
        }
        QTRY_COMPARE(runner->started.load(), 1);
        // The future waits for the release in its destructor.
        const auto release = std::async(std::launch::async, [&runner] {
            QThread::msleep(50);
            runner->hold = false;
        });
        playback.stopAndWait();
        QCOMPARE(runner->ended.load(), 1);
        QCOMPARE(playback.state(), Playback::Stopped);
        playback.setTemporaryDirectory(temporaryOf(dir) / "other");
    }

    // The destructor waits for a render that runs on after it was cancelled.
    void the_destructor_waits_for_the_render() {
        QTemporaryDir dir;
        const auto document = singingDocument(dir);
        QVERIFY(document);
        auto playback = std::make_unique<Playback>(nullptr, temporaryOf(dir), nullptr);
        const auto runner = std::make_shared<SilentRunner>();
        runner->hold = true;
        playback->setRunner(runner);
        kit::DiagnosticList diagnostics;
        if (!playback->play(*document, std::nullopt, someSynthTools(), diagnostics)) {
            runner->hold = false;
            QSKIP("This machine has no audio output device.");
        }
        QTRY_COMPARE(runner->started.load(), 1);
        const auto release = std::async(std::launch::async, [&runner] {
            QThread::msleep(50);
            runner->hold = false;
        });
        playback.reset();
        QCOMPARE(runner->ended.load(), 1);
    }

    // A new temporary directory forgets the kept render, which the next play() renders again.
    void a_new_temporary_directory_forgets_the_kept_render() {
        if (AudioEngine::instance()->sampleRate() <= 0) {
            QSKIP("This machine has no audio output device.");
        }
        QTemporaryDir dir;
        const auto document = singingDocument(dir);
        QVERIFY(document);
        Playback playback(nullptr, temporaryOf(dir), nullptr);
        const auto runner = std::make_shared<SilentRunner>();
        playback.setRunner(runner);
        kit::DiagnosticList diagnostics;
        QVERIFY(playback.play(*document, std::nullopt, someSynthTools(), diagnostics));
        QTRY_COMPARE_WITH_TIMEOUT(playback.state(), Playback::Stopped, 5000);
        QVERIFY(!playback.lastRenderFile().empty());

        playback.stopAndWait();
        const auto other = temporaryOf(dir) / "other";
        fs::create_directories(other);
        playback.setTemporaryDirectory(other);
        QVERIFY(playback.lastRenderFile().empty());
        QVERIFY(playback.play(*document, std::nullopt, someSynthTools(), diagnostics));
        QTRY_COMPARE_WITH_TIMEOUT(playback.state(), Playback::Stopped, 5000);
        QCOMPARE(runner->started.load(), 2);
        QCOMPARE(playback.lastRenderFile(), other / "temp.wav");
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_Playback test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_Playback.moc"
