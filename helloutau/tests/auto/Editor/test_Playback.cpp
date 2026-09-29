#include <atomic>
#include <filesystem>
#include <fstream>

#include <QtCore/QTemporaryDir>
#include <QtCore/QThread>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <hellokit/Edit/ProjectDocument.h>
#include <hellokit/Synth/SynthPlan.h>

#include <helloutau/Audio/AudioOutput.h>

#include <helloutau/Editor/Playback.h>

using namespace hello;
using namespace hello::daw;
namespace fs = std::filesystem;

namespace {

    void writeBytes(const fs::path &path, const QByteArray &bytes) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(bytes.constData(), bytes.size());
    }

    // A tenth of a second of 16-bit mono silence at 44100 Hz
    QByteArray silence() {
        const quint32 data = 4410 * 2;
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

    // Writes silence as the track file instead of running engines, and records the plans.
    class SilentRunner : public kit::SynthRunner {
    public:
        mutable QList<int> stepCounts;
        mutable std::atomic<int> started = 0;
        mutable QList<fs::path> caches;
        mutable std::atomic<bool> waitForCancel = false;

        kit::SynthOutcome render(const kit::SynthPlan &plan, const kit::SynthEngines &engines,
                                 kit::SynthObserver *observer,
                                 kit::DiagnosticList &diagnostics) const override {
            Q_UNUSED(engines);
            Q_UNUSED(diagnostics);
            caches.push_back(plan.cacheDirectory());
            stepCounts.push_back(int(plan.steps().size()));
            ++started;
            kit::SynthOutcome outcome;
            while (waitForCancel.load()) {
                if (observer->cancelled()) {
                    outcome.cancelled = true;
                    return outcome;
                }
                QThread::msleep(5);
            }
            observer->progressed(int(plan.steps().size()), int(plan.steps().size()));
            writeBytes(plan.outputFile(), silence());
            outcome.rendered = true;
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

    kit::SynthEngines someEngines() {
        kit::SynthEngines engines;
        engines.resampler = "resampler.exe";
        engines.wavtool = "wavtool.exe";
        return engines;
    }

}

class test_Playback : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Beside the .usth, beside the UST it came from, or else a temporary directory.
    void the_cache_is_where_utau_keeps_it() {
        QTemporaryDir dir;
        Playback playback;

        kit::ProjectDocument unsaved;
        const auto temporary = playback.cacheDirectoryFor(unsaved);
        QVERIFY(fs::is_directory(temporary));
        QCOMPARE(playback.cacheDirectoryFor(unsaved), temporary);

        const auto document = singingDocument(dir);
        QVERIFY(document);
        QCOMPARE(playback.cacheDirectoryFor(*document),
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
        QCOMPARE(playback.cacheDirectoryFor(*imported),
                 fs::path(dir.path().toStdU16String()) / "imported.cache");
    }

    void playing_needs_engines_and_a_voice_bank() {
        QTemporaryDir dir;
        Playback playback;
        kit::DiagnosticList diagnostics;

        const auto document = singingDocument(dir);
        QVERIFY(document);
        QVERIFY(!playback.play(*document, std::nullopt, {}, diagnostics));
        QVERIFY(kit::hasError(diagnostics));

        diagnostics.clear();
        kit::ProjectDocument silent;
        QVERIFY(!playback.play(silent, std::nullopt, someEngines(), diagnostics));
        QVERIFY(kit::hasError(diagnostics));
        QCOMPARE(playback.state(), Playback::Stopped);
    }

    // The selection is rendered, into the cache of the document, then played to its end.
    void a_render_is_played_to_its_end() {
        if (AudioOutput::deviceSampleRate() <= 0) {
            QSKIP("This machine has no audio output device.");
        }
        QTemporaryDir dir;
        const auto document = singingDocument(dir);
        QVERIFY(document);
        Playback playback;
        const auto runner = std::make_shared<SilentRunner>();
        playback.setRunner(runner);
        QSignalSpy states(&playback, &Playback::stateChanged);
        QSignalSpy progress(&playback, &Playback::progressed);
        QSignalSpy failures(&playback, &Playback::failed);

        kit::DiagnosticList diagnostics;
        QVERIFY(playback.play(*document, std::make_pair(1, 1), someEngines(), diagnostics));
        QCOMPARE(playback.state(), Playback::Rendering);
        QTRY_COMPARE_WITH_TIMEOUT(states.size(), 3, 5000);
        QCOMPARE(failures.size(), 0);
        QCOMPARE(states.at(1).at(0).value<Playback::State>(), Playback::Playing);
        QCOMPARE(states.at(2).at(0).value<Playback::State>(), Playback::Stopped);
        QCOMPARE(progress.size(), 1);
        QCOMPARE(runner->stepCounts, QList<int>{1});
        QCOMPARE(runner->caches, QList<fs::path>{playback.cacheDirectoryFor(*document)});
        QVERIFY(fs::is_regular_file(playback.cacheDirectoryFor(*document) / "playback.wav"));
    }

    // A preview plays from the time asked for to the end, here within the second note, reading
    // the fragments in the cache rather than running the resampler, which here does not exist.
    void a_preview_plays_from_a_time_to_the_end() {
        if (AudioOutput::deviceSampleRate() <= 0) {
            QSKIP("This machine has no audio output device.");
        }
        QTemporaryDir dir;
        const auto document = singingDocument(dir);
        QVERIFY(document);
        Playback playback;

        kit::SynthPlan::Options options;
        options.cacheDirectory = playback.cacheDirectoryFor(*document);
        options.outputFile = options.cacheDirectory / "playback.wav";
        kit::DiagnosticList diagnostics;
        const auto plan = kit::SynthPlan::make(document->session()->snapshot(),
                                               *document->voiceBank(), options, diagnostics);
        QVERIFY(plan);
        fs::create_directories(options.cacheDirectory);
        for (const auto &step : plan->steps()) {
            writeBytes(step.cacheFile, silence());
        }

        QSignalSpy states(&playback, &Playback::stateChanged);
        kit::SynthEngines engines;
        engines.resampler = fs::path(dir.path().toStdU16String()) / "missing.exe";
        QVERIFY(playback.preview(*document, 750.0, engines, diagnostics));
        QCOMPARE(playback.state(), Playback::Playing);
        const auto first = playback.position();
        QVERIFY(first);
        // From 750 ms on, half way through the second note at 120 bpm, and no more ahead than a
        // device that pulls a buffer as it starts has read
        QVERIFY2(*first >= 750 && *first < 1050, qPrintable(QString::number(*first)));

        QTRY_COMPARE_WITH_TIMEOUT(playback.state(), Playback::Stopped, 5000);
        QCOMPARE(states.size(), 2);
        QCOMPARE(playback.pendingNotes(), 0);
        QVERIFY(playback.takePreviewDiagnostics().isEmpty());
    }

    void a_preview_needs_a_resampler() {
        QTemporaryDir dir;
        const auto document = singingDocument(dir);
        QVERIFY(document);
        Playback playback;
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
        Playback playback;
        const auto runner = std::make_shared<SilentRunner>();
        runner->waitForCancel = true;
        playback.setRunner(runner);
        QSignalSpy states(&playback, &Playback::stateChanged);
        QSignalSpy failures(&playback, &Playback::failed);

        kit::DiagnosticList diagnostics;
        if (!playback.play(*document, std::nullopt, someEngines(), diagnostics)) {
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
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_Playback test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_Playback.moc"
