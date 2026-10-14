#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include <QtCore/QtMath>
#include <QtTest/QTest>

#include <hellokit/AudioFile/WaveAudio.h>

using namespace hello::kit;

namespace {

    void appendU16(QByteArray &bytes, quint16 value) {
        bytes.append(char(value & 0xff));
        bytes.append(char(value >> 8));
    }

    void appendU32(QByteArray &bytes, quint32 value) {
        for (int i = 0; i < 4; ++i) {
            bytes.append(char((value >> (8 * i)) & 0xff));
        }
    }

    // A WAVE file with the given format and data. \a extensible writes WAVEFORMATEXTENSIBLE with
    // \a tag as its subformat. \a claimed overrides the size written for the data chunk.
    QByteArray wave(quint16 tag, int channels, int rate, int bits, const QByteArray &data,
                    bool extensible = false, qint64 claimed = -1) {
        QByteArray format;
        appendU16(format, extensible ? 0xFFFE : tag);
        appendU16(format, quint16(channels));
        appendU32(format, quint32(rate));
        appendU32(format, quint32(rate * channels * bits / 8));
        appendU16(format, quint16(channels * bits / 8));
        appendU16(format, quint16(bits));
        if (extensible) {
            appendU16(format, 22);
            appendU16(format, quint16(bits));
            appendU32(format, 0);
            appendU16(format, tag);
            format.append(14, '\0');
        }

        QByteArray body("WAVE");
        // A chunk before the format, which is skipped
        body.append("LIST");
        appendU32(body, 3);
        body.append("abc");
        body.append('\0');
        body.append("fmt ");
        appendU32(body, quint32(format.size()));
        body.append(format);
        body.append("data");
        appendU32(body, quint32(claimed >= 0 ? claimed : data.size()));
        body.append(data);

        QByteArray bytes("RIFF");
        appendU32(bytes, quint32(body.size()));
        bytes.append(body);
        return bytes;
    }

}

class test_WaveAudio : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void sixteen_bit_pcm_as_the_wavtool_writes_it() {
        QByteArray data;
        for (const qint16 value : {qint16(0), qint16(16384), qint16(-32768), qint16(32767)}) {
            appendU16(data, quint16(value));
        }
        DiagnosticList diagnostics;
        const auto audio = WaveAudio::fromBytes(wave(1, 1, 44100, 16, data), diagnostics);
        QVERIFY(audio);
        QVERIFY(diagnostics.empty());
        QCOMPARE(audio->sampleRate, 44100);
        QCOMPARE(audio->channels, 1);
        QCOMPARE(audio->frameCount(), 4);
        QCOMPARE(audio->samples, (std::vector<float>{0.0f, 0.5f, -1.0f, 32767 / 32768.0f}));
    }

    void the_other_sample_formats() {
        DiagnosticList diagnostics;

        // 8 bits are unsigned around 128.
        auto audio =
            WaveAudio::fromBytes(wave(1, 1, 8000, 8, QByteArray("\x80\xc0\x00", 3)), diagnostics);
        QVERIFY(audio);
        QCOMPARE(audio->samples, (std::vector<float>{0.0f, 0.5f, -1.0f}));

        // 24 bits, stereo: one frame of -0.5 and 0.25
        audio = WaveAudio::fromBytes(
            wave(1, 2, 48000, 24, QByteArray("\x00\x00\xc0\x00\x00\x20", 6)), diagnostics);
        QVERIFY(audio);
        QCOMPARE(audio->channels, 2);
        QCOMPARE(audio->samples, (std::vector<float>{-0.5f, 0.25f}));

        // 32-bit float as the subformat of WAVE_FORMAT_EXTENSIBLE
        QByteArray floats(8, '\0');
        const float values[] = {0.75f, -0.125f};
        std::memcpy(floats.data(), values, sizeof values);
        audio = WaveAudio::fromBytes(wave(3, 1, 44100, 32, floats, true), diagnostics);
        QVERIFY(audio);
        QCOMPARE(audio->samples, (std::vector<float>{0.75f, -0.125f}));

        // 64-bit float
        QByteArray doubles(8, '\0');
        const double one = -0.25;
        std::memcpy(doubles.data(), &one, sizeof one);
        audio = WaveAudio::fromBytes(wave(3, 1, 44100, 64, doubles), diagnostics);
        QVERIFY(audio);
        QCOMPARE(audio->samples, std::vector<float>{-0.25f});
        QVERIFY(diagnostics.empty());
        QCOMPARE(audio->duration(), 1000.0 / 44100);
    }

    // A file shorter than its data chunk is read up to its end.
    void a_file_shorter_than_its_data_is_read_to_its_end() {
        QByteArray data;
        appendU16(data, 16384);
        appendU16(data, 16384);
        DiagnosticList diagnostics;
        const auto audio =
            WaveAudio::fromBytes(wave(1, 1, 44100, 16, data, false, 1000), diagnostics);
        QVERIFY(audio);
        QCOMPARE(audio->frameCount(), 2);
        QCOMPARE(diagnostics.size(), 1);
        QCOMPARE(diagnostics.front().severity, DiagnosticSeverity::Warning);
    }

    void malformed_files_are_refused() {
        DiagnosticList diagnostics;
        QVERIFY(!WaveAudio::fromBytes("RIFF", diagnostics));
        QVERIFY(!WaveAudio::fromBytes(QByteArray("RIFF\0\0\0\0AVI ", 12), diagnostics));
        // An unsupported format, and a block size that contradicts the sample size
        QVERIFY(!WaveAudio::fromBytes(wave(2, 1, 44100, 4, QByteArray(4, '\0')), diagnostics));
        auto inconsistent = wave(1, 1, 44100, 16, QByteArray(4, '\0'));
        inconsistent[44] = 3; // the block size, after the header, the LIST chunk and the fmt header
        QVERIFY(!WaveAudio::fromBytes(inconsistent, diagnostics));
        // No channels
        QVERIFY(!WaveAudio::fromBytes(wave(1, 0, 44100, 16, QByteArray(4, '\0')), diagnostics));
        QVERIFY(hasError(diagnostics));
    }

    // The sample format of the file is recorded: the bits, floating point, an extensible header
    // and a data chunk longer than the file.
    void the_sample_format_is_recorded() {
        DiagnosticList diagnostics;
        auto audio = WaveAudio::fromBytes(wave(1, 2, 48000, 24, QByteArray(6, '\0')), diagnostics);
        QVERIFY(audio);
        QCOMPARE(audio->bitsPerSample, 24);
        QVERIFY(!audio->floatingPoint);
        QVERIFY(!audio->extensible);
        QVERIFY(!audio->truncated);

        audio = WaveAudio::fromBytes(wave(3, 1, 44100, 32, QByteArray(8, '\0'), true), diagnostics);
        QVERIFY(audio);
        QVERIFY(audio->floatingPoint);
        QVERIFY(audio->extensible);

        audio = WaveAudio::fromBytes(wave(1, 1, 44100, 16, QByteArray(4, '\0'), false, 1000),
                                     diagnostics);
        QVERIFY(audio);
        QVERIFY(audio->truncated);
    }

    // The format chunk and the data chunk read back as the same audio in every sample format,
    // with a plain header.
    void the_audio_is_written_in_its_sample_format() {
        const std::vector<float> samples{0.0f, 0.5f, -1.0f, 0.25f};
        for (const auto &[bits, floating] : std::initializer_list<std::pair<int, bool>>{
                 {8,  false},
                 {16, false},
                 {24, false},
                 {32, false},
                 {32, true },
                 {64, true }
        }) {
            WaveAudio audio;
            audio.sampleRate = 22050;
            audio.channels = 2;
            audio.bitsPerSample = bits;
            audio.floatingPoint = floating;
            audio.samples = samples;
            const auto format = audio.formatChunk();
            QCOMPARE(format.size(), floating ? 18 : 16);
            DiagnosticList diagnostics;
            const auto read = WaveAudio::fromBytes(
                wave(floating ? 3 : 1, 2, 22050, bits, audio.dataChunk()), diagnostics);
            QVERIFY(read);
            QCOMPARE(read->samples, samples);
            QCOMPARE(format.left(16), wave(floating ? 3 : 1, 2, 22050, bits, {}).mid(32, 16));
        }
    }

    // Integer samples are rounded to the nearest value and limited to the range of the format.
    void integer_samples_are_rounded_and_limited() {
        WaveAudio audio;
        audio.sampleRate = 44100;
        audio.channels = 1;
        audio.samples = {1.5f, -2.0f, 0.4f / 32768, 0.6f / 32768};
        QCOMPARE(audio.dataChunk(), QByteArray("\xff\x7f\x00\x80\x00\x00\x01\x00", 8));
    }

    // A tone keeps its pitch, its duration and its time across a change of the sample rate.
    void resampling_keeps_pitch_and_duration() {
        constexpr int from = 44100;
        constexpr int to = 48000;
        constexpr double frequency = 440;
        std::vector<float> tone(from);
        for (size_t i = 0; i < tone.size(); ++i) {
            tone[i] = float(0.5 * std::sin(2 * M_PI * frequency * double(i) / from));
        }

        const auto converted = WaveAudio::resampled(tone, 1, from, to);
        QCOMPARE(qsizetype(converted.size()), qsizetype(to));
        // Away from the edges, where the filter has no neighbors, the result is the same tone.
        double error = 0;
        for (int i = to / 4; i < to * 3 / 4; ++i) {
            const double expected = 0.5 * std::sin(2 * M_PI * frequency * i / to);
            error = std::max(error, std::abs(converted[size_t(i)] - expected));
        }
        QVERIFY2(error < 1e-3, qPrintable(QString::number(error)));

        // The samples pass through unchanged at the same rate, and an empty input stays empty.
        QCOMPARE(WaveAudio::resampled(tone, 1, from, from), tone);
        QVERIFY(WaveAudio::resampled({}, 1, from, to).empty());
    }

    // Interleaved channels are converted separately. The audio keeps its other members.
    void channels_are_resampled_separately() {
        WaveAudio audio;
        audio.sampleRate = 44100;
        audio.channels = 2;
        audio.bitsPerSample = 24;
        for (int i = 0; i < 4410; ++i) {
            audio.samples.push_back(0.25f);
            audio.samples.push_back(-0.5f);
        }
        const auto converted = audio.resampled(22050);
        QCOMPARE(converted.sampleRate, 22050);
        QCOMPARE(converted.bitsPerSample, 24);
        QCOMPARE(qsizetype(converted.samples.size()), qsizetype(2 * 2205));
        QVERIFY(std::abs(converted.samples[2 * 1000] - 0.25f) < 1e-3);
        QVERIFY(std::abs(converted.samples[2 * 1000 + 1] + 0.5f) < 1e-3);
    }
};

QTEST_APPLESS_MAIN(test_WaveAudio)

#include "test_WaveAudio.moc"
