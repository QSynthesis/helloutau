#include <cmath>

#include <QtTest/QTest>

#include <hellokit/Synth/Spectrogram.h>

using namespace hello::kit;

namespace {

    // A second of a sine of frequency at 44100 Hz, amplitude as given, in channels copies
    WaveAudio sine(double frequency, double amplitude, int channels = 1) {
        WaveAudio audio;
        audio.sampleRate = 44100;
        audio.channels = channels;
        for (int i = 0; i < 44100; ++i) {
            const auto value =
                float(amplitude * std::sin(2 * 3.14159265358979 * frequency * i / 44100));
            for (int c = 0; c < channels; ++c) {
                audio.samples.push_back(value);
            }
        }
        return audio;
    }

    int loudestBin(const Spectrogram &spectrogram, int frame) {
        int loudest = 0;
        for (int bin = 1; bin < Spectrogram::binCount; ++bin) {
            if (spectrogram.magnitude(frame, bin) > spectrogram.magnitude(frame, loudest)) {
                loudest = bin;
            }
        }
        return loudest;
    }

}

class test_Spectrogram : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // A sine shows at its frequency, at the level of its amplitude.
    void a_sine_shows_at_its_frequency() {
        const auto spectrogram = Spectrogram::of(sine(440, 0.5));
        QCOMPARE(spectrogram.sampleRate(), 44100);
        QCOMPARE(spectrogram.frameCount(), 44100 / 512 + 1);
        QCOMPARE(spectrogram.timeOf(2), 1024 * 1000 / 44100.0);
        const int middle = spectrogram.frameCount() / 2;
        const int bin = loudestBin(spectrogram, middle);
        QVERIFY(std::abs(spectrogram.frequencyOf(bin) - 440) < 44100.0 / 2048);
        QCOMPARE(spectrogram.binOf(spectrogram.frequencyOf(20.5)), 20.5);
        QVERIFY(std::abs(spectrogram.magnitude(middle, bin) - 0.5) < 0.1);
        QVERIFY(spectrogram.peak() >= spectrogram.magnitude(middle, bin));
        QVERIFY(spectrogram.magnitude(middle, bin + 20) < 0.01);

        // The channels are mixed; beyond the frames and bins the magnitude is 0.
        const auto stereo = Spectrogram::of(sine(1000, 0.5, 2));
        QVERIFY(std::abs(stereo.frequencyOf(loudestBin(stereo, middle)) - 1000) < 44100.0 / 2048);
        QCOMPARE(stereo.magnitude(-1, 10), 0.0f);
        QCOMPARE(stereo.magnitude(0, Spectrogram::binCount), 0.0f);
    }

    void silence_is_empty() {
        const auto spectrogram = Spectrogram::of(WaveAudio());
        QCOMPARE(spectrogram.frameCount(), 0);
        QCOMPARE(spectrogram.peak(), 0.0f);
    }
};

QTEST_APPLESS_MAIN(test_Spectrogram)

#include "test_Spectrogram.moc"
