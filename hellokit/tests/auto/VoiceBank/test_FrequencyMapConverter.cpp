#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/AudioFile/WaveAudio.h>
#include <hellokit/VoiceBank/BuiltinFrequencyFormats.h>
#include <hellokit/VoiceBank/FrequencyFormats.h>
#include <hellokit/VoiceBank/FrequencyMapConverter.h>

using namespace hello::kit;

namespace fs = std::filesystem;

namespace {

    // A format with the hop of its resampler, which reports the map of the format for every
    // audio file whose name begins with "a"
    class Fixed : public FrequencyFormat {
    public:
        double hop = 256;
        FrequencyMap map;

        QString id() const override {
            return QStringLiteral("fixed");
        }
        QString name() const override {
            return QStringLiteral("fixed");
        }
        QStringList resamplerPatterns() const override {
            return {};
        }
        QList<bool> mapsExist(const fs::path &, const QList<fs::path> &wavs) const override {
            QList<bool> found;
            for (const auto &wav : wavs) {
                found.push_back(wav.u16string().front() == u'a');
            }
            return found;
        }
        std::optional<FrequencyMap> read(const fs::path &, int, DiagnosticList &) const override {
            return map;
        }
        double hopOf(int) const override {
            return hop;
        }
    };

    // The audio of two channels with the samples of the first channel, at 1000 Hz
    WaveAudio audioOf(std::initializer_list<float> samples) {
        WaveAudio audio;
        audio.sampleRate = 1000;
        audio.channels = 2;
        for (const float sample : samples) {
            audio.samples.push_back(sample);
            audio.samples.push_back(1);
        }
        return audio;
    }

    // A map of frames hop samples apart at 1000 Hz with the frequencies and no amplitudes
    FrequencyMap mapOf(double hop, std::initializer_list<double> frequencies) {
        FrequencyMap map;
        map.hop = hop;
        for (const double frequency : frequencies) {
            FrequencyMap::Frame frame;
            frame.time = double(map.frames.size()) * hop;
            frame.frequency = frequency;
            map.frames.push_back(frame);
        }
        return map;
    }

    void appendU16(QByteArray &bytes, quint16 value) {
        bytes.append(char(value & 0xff));
        bytes.append(char(value >> 8));
    }

    void appendU32(QByteArray &bytes, quint32 value) {
        for (int i = 0; i < 4; ++i) {
            bytes.append(char((value >> (8 * i)) & 0xff));
        }
    }

    // Writes a mono 16-bit WAVE file at rate, with count samples of the value sample
    void writeWave(const fs::path &path, int rate, int count, qint16 sample) {
        QByteArray data;
        for (int i = 0; i < count; ++i) {
            appendU16(data, quint16(sample));
        }
        QByteArray bytes("RIFF");
        appendU32(bytes, quint32(36 + data.size()));
        bytes.append("WAVEfmt ");
        appendU32(bytes, 16);
        appendU16(bytes, 1);
        appendU16(bytes, 1);
        appendU32(bytes, quint32(rate));
        appendU32(bytes, quint32(rate * 2));
        appendU16(bytes, 2);
        appendU16(bytes, 16);
        bytes.append("data");
        appendU32(bytes, quint32(data.size()));
        bytes.append(data);
        QFile file(QString::fromStdU16String(path.u16string()));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(bytes);
    }

}

class test_FrequencyMapConverter : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The frames of a map with the hop of the target format are unchanged. The missing amplitudes
    // are the root mean square of the first channel on the scale of 16-bit samples.
    void the_frames_of_a_map_with_the_hop_of_the_target_are_unchanged() {
        Fixed target;
        target.hop = 2;
        const auto map = mapOf(2, {100, 0, 200});
        const auto audio = audioOf({0.5f, -0.5f, 0.25f, 0.25f});
        const auto converted = FrequencyMapConverter::converted(map, target, audio);
        QCOMPARE(converted.frames.size(), size_t(3));
        QCOMPARE(converted.frames.at(2).frequency, 200.0);
        QCOMPARE(*converted.frames.at(0).amplitude, 16384.0);
        QCOMPARE(*converted.frames.at(1).amplitude, 8192.0);
        // The frame after the audio
        QCOMPARE(*converted.frames.at(2).amplitude, 0.0);
    }

    // The amplitudes of a map that has amplitudes are kept.
    void the_amplitudes_of_a_map_are_kept() {
        Fixed target;
        target.hop = 2;
        auto map = mapOf(2, {100});
        map.frames[0].amplitude = 7;
        const auto converted = FrequencyMapConverter::converted(map, target, audioOf({0.5f}));
        QCOMPARE(*converted.frames.at(0).amplitude, 7.0);
    }

    // A map with another hop is resampled to floor(n / hop) + 1 frames of the hop of the target
    // format, where n is the number of audio frames, and the amplitudes are computed for the new
    // frames.
    void a_map_with_another_hop_is_resampled() {
        Fixed target;
        target.hop = 3;
        const auto map = mapOf(2, {100, 200, 300, 400});
        const auto audio = audioOf({0.5f, 0.5f, 0.5f, 0, 0, 0, 0});
        const auto converted = FrequencyMapConverter::converted(map, target, audio);
        QCOMPARE(converted.hop, 3.0);
        QCOMPARE(converted.frames.size(), size_t(3));
        QCOMPARE(converted.frames.at(1).time, 3.0);
        QCOMPARE(converted.frames.at(1).frequency, 250.0);
        QCOMPARE(converted.frames.at(2).frequency, 400.0);
        QCOMPARE(*converted.frames.at(0).amplitude, 16384.0);
        QCOMPARE(*converted.frames.at(1).amplitude, 0.0);
    }

    // The maps of the source format are written as frq with the hop of resampler.exe and the
    // amplitudes of the audio. An audio file without a map of the source format is skipped.
    void the_maps_are_written_in_the_target_format() {
        QTemporaryDir temp;
        const fs::path directory(temp.path().toStdU16String());
        writeWave(directory / "a.wav", 48000, 1024, 16384);
        writeWave(directory / "b.wav", 48000, 1024, 16384);

        FrequencyFormats formats;
        const BuiltinFrequencyFormats builtins(formats.registry());
        const auto frq = formats.format(QStringLiteral("frq"));
        Fixed source;
        source.map = mapOf(5.80498866213152 * 48, {100, 200, 300});

        DiagnosticList diagnostics;
        const auto outcome = FrequencyMapConverter::convert(source, *frq, directory,
                                                            {"a.wav", "b.wav"}, diagnostics);
        QVERIFY(diagnostics.isEmpty());
        QCOMPARE(outcome.converted, 1);
        QCOMPARE(outcome.resampled, 1);
        QVERIFY(!fs::exists(directory / "b_wav.frq"));

        const auto written = frq->read(directory / "a.wav", 48000, diagnostics);
        QVERIFY(written);
        QCOMPARE(written->hop, 256.0);
        QCOMPARE(written->frames.size(), size_t(5));
        QCOMPARE(written->frames.at(0).frequency, 100.0);
        QCOMPARE(*written->frames.at(0).amplitude, 16384.0);
    }

    // An audio file that cannot be read is reported, and the other maps are converted.
    void an_unreadable_audio_file_is_reported() {
        QTemporaryDir temp;
        const fs::path directory(temp.path().toStdU16String());
        writeWave(directory / "a.wav", 44100, 512, 0);

        FrequencyFormats formats;
        const BuiltinFrequencyFormats builtins(formats.registry());
        Fixed source;
        source.map = mapOf(256, {100, 200, 300});

        DiagnosticList diagnostics;
        const auto outcome =
            FrequencyMapConverter::convert(source, *formats.format(QStringLiteral("frq")),
                                           directory, {"a.wav", "absent.wav"}, diagnostics);
        QCOMPARE(diagnostics.size(), 1);
        QCOMPARE(outcome.converted, 1);
        QCOMPARE(outcome.resampled, 0);
        QVERIFY(fs::exists(directory / "a_wav.frq"));
    }
};

QTEST_GUILESS_MAIN(test_FrequencyMapConverter)

#include "test_FrequencyMapConverter.moc"
