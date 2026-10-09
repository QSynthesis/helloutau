#include <filesystem>
#include <fstream>
#include <mutex>

#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <helloutau/Editor/SamplePreview.h>

using namespace hello;
using namespace hello::daw;
namespace fs = std::filesystem;

namespace {

    // A WAVE file of 16-bit mono PCM at 1000 Hz, one frame per millisecond
    void writeWave(const fs::path &path, int frames) {
        QByteArray bytes;
        const auto put32 = [&bytes](quint32 value) {
            for (int i = 0; i < 4; ++i) {
                bytes.push_back(char((value >> (8 * i)) & 0xff));
            }
        };
        const auto put16 = [&bytes](quint16 value) {
            bytes.push_back(char(value & 0xff));
            bytes.push_back(char(value >> 8));
        };
        bytes.append("RIFF");
        put32(quint32(36 + frames * 2));
        bytes.append("WAVEfmt ");
        put32(16);
        put16(1);
        put16(1);
        put32(1000);
        put32(2000);
        put16(2);
        put16(16);
        bytes.append("data");
        put32(quint32(frames * 2));
        for (int i = 0; i < frames; ++i) {
            put16(quint16(i % 2 ? 8000 : -8000));
        }
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(bytes.constData(), bytes.size());
    }

    // A resampler that copies its input to its output, or writes nothing
    class FakeResampler : public kit::SynthToolProcess {
    public:
        bool writes = true;

        kit::SynthToolRun run(const std::filesystem::path &program, const QStringList &arguments,
                              kit::DiagnosticList &, const std::function<bool()> &) const override {
            {
                const std::lock_guard lock(m_mutex);
                m_program = program;
                m_arguments = arguments;
            }
            if (writes) {
                std::error_code error;
                fs::copy_file(fs::path(arguments.at(0).toStdU16String()),
                              fs::path(arguments.at(1).toStdU16String()),
                              fs::copy_options::overwrite_existing, error);
            }
            kit::SynthToolRun run;
            run.started = true;
            run.output = QStringLiteral("resampler output");
            return run;
        }

        QStringList arguments() const {
            const std::lock_guard lock(m_mutex);
            return m_arguments;
        }

    private:
        mutable std::mutex m_mutex;
        mutable fs::path m_program;
        mutable QStringList m_arguments;
    };

    kit::VoiceSample sampleIn(const QTemporaryDir &dir) {
        kit::VoiceSample sample;
        sample.path = fs::path(dir.path().toStdU16String()) / "a.wav";
        sample.fileName = QStringLiteral("a.wav");
        sample.alias = QStringLiteral("R");
        sample.offset = 10;
        sample.consonant = 50;
        sample.cutoff = -300;
        sample.preUtterance = 40;
        sample.voiceOverlap = 5;
        writeWave(sample.path, 1000);
        return sample;
    }

}

class test_SamplePreview : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Of the keys whose affixes enclose the alias, or whose prefix is the folder, the middle
    // one; C4 otherwise.
    void the_pitch_is_that_of_the_folder_in_the_prefix_map() {
        QMap<int, kit::VoicePrefix> map;
        for (int key = 48; key < 84; ++key) {
            kit::VoicePrefix prefix;
            prefix.suffix = key < 60    ? QStringLiteral("_L")
                            : key >= 72 ? QStringLiteral("_H")
                                        : QString();
            map.insert(key, prefix);
        }
        QCOMPARE(SamplePreview::noteNumFor(map, {}, QStringLiteral("a_H")), 78);
        QCOMPARE(SamplePreview::noteNumFor(map, {}, QStringLiteral("a_L")), 54);
        QCOMPARE(SamplePreview::noteNumFor(map, {}, QStringLiteral("a")), 60);
        // The affix alone is no alias of the folder.
        QCOMPARE(SamplePreview::noteNumFor(map, {}, QStringLiteral("_H")), 60);

        QMap<int, kit::VoicePrefix> folders;
        for (int key = 50; key < 62; ++key) {
            folders.insert(key, {QStringLiteral("low\\"), {}});
        }
        QCOMPARE(SamplePreview::noteNumFor(folders, fs::path("low"), QStringLiteral("a")), 56);
        QCOMPARE(SamplePreview::noteNumFor(folders, fs::path("high"), QStringLiteral("a")), 60);
        QCOMPARE(SamplePreview::noteNumFor({}, fs::path("low"), QStringLiteral("a")), 60);
    }

    // The note is the sample under a lyric of its own, so that even the alias R, a rest as a
    // lyric, is sung; the resampler gets the pitch asked for.
    void a_note_is_synthesized_by_the_resampler_alone() {
        QTemporaryDir dir;
        const auto sample = sampleIn(dir);
        SamplePreview preview;
        const auto synthTool = std::make_shared<FakeResampler>();
        preview.setSynthToolProcess(synthTool);
        QSignalSpy failed(&preview, &SamplePreview::failed);

        kit::DiagnosticList diagnostics;
        QVERIFY(preview.synthesize(sample, 62, 480, fs::path("resampler.exe"), diagnostics));
        QCOMPARE(preview.state(), SamplePreview::Synthesizing);
        QTRY_VERIFY(preview.state() != SamplePreview::Synthesizing);
        QVERIFY(preview.synthesized());
        QCOMPARE(preview.synthesized()->sampleRate, 1000);
        const auto arguments = synthTool->arguments();
        QCOMPARE(fs::path(arguments.at(0).toStdU16String()), sample.path);
        QCOMPARE(arguments.at(2), QStringLiteral("D4"));
        preview.stop();
        QCOMPARE(preview.state(), SamplePreview::Stopped);
    }

    void a_resampler_that_writes_nothing_fails() {
        QTemporaryDir dir;
        const auto sample = sampleIn(dir);
        SamplePreview preview;
        const auto synthTool = std::make_shared<FakeResampler>();
        synthTool->writes = false;
        preview.setSynthToolProcess(synthTool);
        QSignalSpy failed(&preview, &SamplePreview::failed);

        kit::DiagnosticList diagnostics;
        QVERIFY(preview.synthesize(sample, 60, 480, fs::path("resampler.exe"), diagnostics));
        QTRY_COMPARE(failed.size(), 1);
        QCOMPARE(preview.state(), SamplePreview::Stopped);
        const auto reasons = failed.at(0).at(0).value<kit::DiagnosticList>();
        QVERIFY(reasons.last().message.contains(QStringLiteral("resampler output")));
        QVERIFY(!preview.synthesized());

        // Without a resampler nothing starts.
        diagnostics.clear();
        QVERIFY(!preview.synthesize(sample, 60, 480, {}, diagnostics));
        QCOMPARE(diagnostics.size(), 1);
    }

    void nothing_plays_outside_the_audio() {
        SamplePreview preview;
        auto audio = std::make_shared<kit::WaveAudio>();
        audio->sampleRate = 1000;
        audio->channels = 1;
        audio->samples.assign(1000, 0.1f);
        kit::DiagnosticList diagnostics;
        QVERIFY(!preview.play(audio, 1200, std::nullopt, diagnostics));
        QCOMPARE(diagnostics.size(), 1);
        QVERIFY(!preview.play(nullptr, 0, std::nullopt, diagnostics));
        QCOMPARE(preview.state(), SamplePreview::Stopped);
    }
};

int main(int argc, char *argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_SamplePreview test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_SamplePreview.moc"
