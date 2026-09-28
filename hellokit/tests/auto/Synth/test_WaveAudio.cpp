#include <cstring>

#include <QtTest/QTest>

#include <hellokit/Synth/WaveAudio.h>

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

    // A WAVE file with the given format and data; \a extensible writes WAVEFORMATEXTENSIBLE with
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

    // A file cut short keeps the audio it has.
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
};

QTEST_APPLESS_MAIN(test_WaveAudio)

#include "test_WaveAudio.moc"
