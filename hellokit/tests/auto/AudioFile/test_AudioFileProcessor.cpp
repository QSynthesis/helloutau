#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <string>

#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/AudioFile/AudioFileProcessor.h>
#include <hellokit/AudioFile/WaveAudio.h>

using namespace hello::kit;
namespace fs = std::filesystem;

namespace {

    using Processor = AudioFileProcessor;

    QByteArray u32(quint32 value) {
        QByteArray bytes;
        for (int i = 0; i < 4; ++i) {
            bytes.push_back(char((value >> (8 * i)) & 0xff));
        }
        return bytes;
    }

    QByteArray chunk(const char *id, const QByteArray &contents) {
        auto bytes = QByteArray(id, 4) + u32(quint32(contents.size())) + contents;
        if (contents.size() % 2) {
            bytes.push_back('\0');
        }
        return bytes;
    }

    QByteArray riff(const QByteArray &chunks) {
        return "RIFF" + u32(quint32(4 + chunks.size())) + "WAVE" + chunks;
    }

    // Audio of the given format with the samples, interleaved
    WaveAudio audioOf(int rate, int channels, int bits, bool floating, std::vector<float> samples) {
        WaveAudio audio;
        audio.sampleRate = rate;
        audio.channels = channels;
        audio.bitsPerSample = bits;
        audio.floatingPoint = floating;
        audio.samples = std::move(samples);
        return audio;
    }

    // The WAVE file of audio with a plain header and the chunks after the audio
    QByteArray fileOf(const WaveAudio &audio, const QByteArray &after = {}) {
        return riff(chunk("fmt ", audio.formatChunk()) + chunk("data", audio.dataChunk()) + after);
    }

    // The format chunk of audio as WAVE_FORMAT_EXTENSIBLE
    QByteArray extensibleFormatOf(const WaveAudio &audio) {
        auto format = audio.formatChunk().left(16);
        format[0] = char(0xfe);
        format[1] = char(0xff);
        format += QByteArray::fromHex("1600") + format.mid(14, 2) + u32(0) +
                  QByteArray(audio.floatingPoint ? "\x03\x00" : "\x01\x00", 2) +
                  QByteArray::fromHex("000000001000800000aa00389b71");
        return format;
    }

    WaveAudio read(const QByteArray &bytes) {
        DiagnosticList diagnostics;
        auto audio = WaveAudio::fromBytes(bytes, diagnostics);
        return audio ? *audio : WaveAudio();
    }

    std::vector<float> constant(qsizetype count, float value) {
        return std::vector<float>(size_t(count), value);
    }

}

class test_AudioFileProcessor : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Each property of the file that differs from the options is a change, and the default
    // options require no change of a plain 16-bit mono file at 44100 Hz.
    void the_changes_follow_the_options() {
        DiagnosticList diagnostics;
        const auto plain = audioOf(44100, 1, 16, false, constant(100, 0.25f));
        auto inspection = Processor::inspect(fileOf(plain), diagnostics);
        QVERIFY(inspection);
        QCOMPARE(inspection->changes({}), Processor::Changes());
        QCOMPARE(inspection->peak, 0.25);

        const auto stereo = audioOf(48000, 2, 24, false, constant(200, 0.25f));
        auto file = riff(chunk("fmt ", extensibleFormatOf(stereo)) + chunk("LIST", "INFO") +
                         "data" + u32(4000) + stereo.dataChunk());
        inspection = Processor::inspect(file, diagnostics);
        QVERIFY(inspection);
        QVERIFY(diagnostics.isEmpty());
        QCOMPARE(inspection->changes({}), Processor::SampleRateChange | Processor::ChannelChange |
                                              Processor::BitDepthChange |
                                              Processor::MetadataChange | Processor::HeaderChange |
                                              Processor::DataSizeChange);

        Processor::Options none;
        none.sampleRate.reset();
        none.mono.reset();
        none.sixteenBits = false;
        none.removingMetadata = false;
        none.plainHeader = false;
        QCOMPARE(inspection->changes(none), Processor::Changes(Processor::DataSizeChange));
    }

    // The DC offset counts from the threshold, and the peak beyond the tolerance.
    void the_offset_and_the_peak_have_thresholds() {
        Processor::Inspection inspection;
        inspection.sampleRate = 44100;
        inspection.channels = 1;
        inspection.bitsPerSample = 16;
        inspection.peak = std::pow(10.0, -1.05 / 20);
        Processor::Options options;
        options.removingDcOffset = true;
        options.peakLevel = -1;
        inspection.dcOffset = Processor::dcOffsetThreshold / 2;
        QCOMPARE(inspection.changes(options), Processor::Changes());
        inspection.dcOffset = Processor::dcOffsetThreshold;
        inspection.peak = std::pow(10.0, -1.2 / 20);
        QCOMPARE(inspection.changes(options), Processor::DcOffsetChange | Processor::PeakChange);
    }

    // The default makes one 16-bit channel at 44100 Hz of the same duration with a plain header
    // and no metadata.
    void the_default_writes_the_recommended_format() {
        std::vector<float> samples;
        for (int i = 0; i < 4800; ++i) {
            samples.push_back(0.5f);
            samples.push_back(0.25f);
        }
        const auto stereo = audioOf(48000, 2, 32, true, samples);
        DiagnosticList diagnostics;
        const auto result =
            Processor::processed(fileOf(stereo, chunk("id3 ", "ID3")), {}, diagnostics);
        QVERIFY(result);
        QVERIFY(diagnostics.isEmpty());
        const auto audio = read(*result);
        QCOMPARE(audio.sampleRate, 44100);
        QCOMPARE(audio.channels, 1);
        QCOMPARE(audio.bitsPerSample, 16);
        QVERIFY(!audio.floatingPoint);
        QCOMPARE(audio.frameCount(), 4410);
        // The average of the channels away from the edges of the filter, within the dither
        QVERIFY(std::abs(audio.samples[2205] - 0.375f) < 3.0f / 32768);
        QVERIFY(!result->contains("id3 "));
    }

    // Without a change of the samples, the audio bytes stay as they are and no dither is added.
    void metadata_alone_keeps_the_samples() {
        std::vector<float> samples;
        for (int i = 0; i < 100; ++i) {
            samples.push_back(float(i - 50) / 32768);
        }
        const auto plain = audioOf(44100, 1, 16, false, samples);
        DiagnosticList diagnostics;
        const auto result =
            Processor::processed(fileOf(plain, chunk("LIST", "INFOISFT")), {}, diagnostics);
        QVERIFY(result);
        QCOMPARE(*result, fileOf(plain));
    }

    // A plain header replaces an extensible header without a change of the samples.
    void an_extensible_header_becomes_plain() {
        const auto audio = audioOf(44100, 1, 16, false, constant(10, 0.5f));
        const auto file =
            riff(chunk("fmt ", extensibleFormatOf(audio)) + chunk("data", audio.dataChunk()));
        DiagnosticList diagnostics;
        QCOMPARE(*Processor::processed(file, {}, diagnostics), fileOf(audio));
    }

    // The metadata is kept unless its removal is set, even if the samples change.
    void the_metadata_is_kept_unless_removed() {
        const auto stereo = audioOf(44100, 2, 16, false, {0.5f, 0.0f, 0.5f, 0.0f});
        Processor::Options options;
        options.removingMetadata = false;
        options.dithered = false;
        DiagnosticList diagnostics;
        const auto result =
            Processor::processed(fileOf(stereo, chunk("LIST", "INFO")), options, diagnostics);
        QVERIFY(result);
        QVERIFY(result->contains("LIST"));
        QCOMPARE(read(*result).samples, (std::vector<float>{0.25f, 0.25f}));
    }

    // One channel of stereo audio is taken, the left or the right.
    void a_channel_is_taken() {
        const auto stereo = audioOf(44100, 2, 16, false, {0.5f, -0.25f, 0.5f, -0.25f});
        Processor::Options options;
        options.dithered = false;
        DiagnosticList diagnostics;
        options.mono = Processor::FirstChannel;
        QCOMPARE(read(*Processor::processed(fileOf(stereo), options, diagnostics)).samples,
                 (std::vector<float>{0.5f, 0.5f}));
        options.mono = Processor::SecondChannel;
        QCOMPARE(read(*Processor::processed(fileOf(stereo), options, diagnostics)).samples,
                 (std::vector<float>{-0.25f, -0.25f}));
    }

    // The mean of each channel is subtracted, and the peak is scaled to the level.
    void the_offset_and_the_level_are_corrected() {
        const auto audio = audioOf(44100, 1, 32, true, {0.6f, 0.2f, 0.6f, 0.2f});
        Processor::Options options;
        options.sixteenBits = false;
        options.removingDcOffset = true;
        options.peakLevel = -6;
        DiagnosticList diagnostics;
        const auto result = read(*Processor::processed(fileOf(audio), options, diagnostics));
        QVERIFY(result.floatingPoint);
        const float peak = float(std::pow(10.0, -6.0 / 20));
        QCOMPARE(result.samples, (std::vector<float>{peak, -peak, peak, -peak}));
    }

    // The dither changes a rounded sample by at most one step, and a file processed twice is
    // written alike.
    void the_dither_is_one_step_and_repeatable() {
        std::vector<float> samples;
        for (int i = 0; i < 1000; ++i) {
            samples.push_back(float(0.3 * std::sin(i * 0.01)));
        }
        const auto file = fileOf(audioOf(44100, 1, 32, true, samples));
        DiagnosticList diagnostics;
        const auto dithered = Processor::processed(file, {}, diagnostics);
        QCOMPARE(*Processor::processed(file, {}, diagnostics), *dithered);
        Processor::Options options;
        options.dithered = false;
        const auto plain = read(*Processor::processed(file, options, diagnostics)).samples;
        const auto noisy = read(*dithered).samples;
        QCOMPARE(noisy.size(), plain.size());
        bool differs = false;
        for (size_t i = 0; i < plain.size(); ++i) {
            QVERIFY(std::abs(noisy[i] - plain[i]) <= 1.0f / 32768 + 1e-9f);
            differs = differs || noisy[i] != plain[i];
            QCOMPARE(plain[i], std::round(samples[i] * 32768) / 32768);
        }
        QVERIFY(differs);
    }

    // Integer samples beyond full scale are limited, with a warning.
    void samples_beyond_full_scale_are_reported() {
        const auto file = fileOf(audioOf(44100, 1, 32, true, {1.5f, 0.0f}));
        Processor::Options options;
        options.dithered = false;
        DiagnosticList diagnostics;
        const auto result = Processor::processed(file, options, diagnostics);
        QVERIFY(result);
        QCOMPARE(diagnostics.size(), 1);
        QCOMPARE(diagnostics.front().severity, Diagnostic::Warning);
        QCOMPARE(read(*result).samples.front(), 32767.0f / 32768);
    }

    // The file is replaced in place. A file that cannot be read is reported.
    void a_file_is_processed_in_place() {
        QTemporaryDir dir;
        const fs::path root(dir.path().toStdU16String());
        const auto file = root / "a.wav";
        const auto original = fileOf(audioOf(48000, 1, 16, false, constant(480, 0.25f)));
        {
            QFile out(QString::fromStdU16String(file.u16string()));
            QVERIFY(out.open(QIODevice::WriteOnly));
            out.write(original);
        }
        DiagnosticList diagnostics;
        QVERIFY(Processor::process(file, {}, diagnostics));
        QVERIFY(diagnostics.isEmpty());
        QFile in(QString::fromStdU16String(file.u16string()));
        QVERIFY(in.open(QIODevice::ReadOnly));
        QCOMPARE(read(in.readAll()).sampleRate, 44100);

        QVERIFY(!Processor::process(root / "absent.wav", {}, diagnostics));
        QCOMPARE(diagnostics.size(), 1);
    }

    // Files are read on several threads, with the results and the reasons in the order of the
    // files, and the progress counts every file. After a cancellation no further file is read.
    void files_are_inspected_on_threads() {
        QTemporaryDir dir;
        const fs::path root(dir.path().toStdU16String());
        QList<fs::path> files;
        for (int i = 0; i < 6; ++i) {
            const auto file = root / ("f" + std::to_string(i) + ".wav");
            files.push_back(file);
            if (i == 2 || i == 4) {
                continue;
            }
            QFile out(QString::fromStdU16String(file.u16string()));
            QVERIFY(out.open(QIODevice::WriteOnly));
            out.write(fileOf(audioOf(32000 + i, 1, 16, false, constant(10, 0.25f))));
        }

        DiagnosticList diagnostics;
        std::atomic<int> calls = 0;
        std::atomic<int> highest = 0;
        const auto inspections = Processor::inspectFiles(files, 3, diagnostics, [&](int done) {
            ++calls;
            int seen = highest.load();
            while (done > seen && !highest.compare_exchange_weak(seen, done)) {
            }
        });
        QCOMPARE(inspections.size(), 6);
        for (int i = 0; i < 6; ++i) {
            QCOMPARE(inspections.at(i).has_value(), i != 2 && i != 4);
            if (inspections.at(i)) {
                QCOMPARE(inspections.at(i)->sampleRate, 32000 + i);
            }
        }
        QCOMPARE(diagnostics.size(), 2);
        QVERIFY(diagnostics.at(0).message.contains(QStringLiteral("f2.wav")));
        QVERIFY(diagnostics.at(1).message.contains(QStringLiteral("f4.wav")));
        QCOMPARE(calls.load(), 6);
        QCOMPARE(highest.load(), 6);

        DiagnosticList none;
        const auto canceled = Processor::inspectFiles(files, 0, none, {}, [] { return true; });
        QCOMPARE(canceled.size(), 6);
        QVERIFY(std::none_of(canceled.cbegin(), canceled.cend(),
                             [](const auto &inspection) { return inspection.has_value(); }));
        QVERIFY(none.isEmpty());
    }
};

QTEST_APPLESS_MAIN(test_AudioFileProcessor)

#include "test_AudioFileProcessor.moc"
