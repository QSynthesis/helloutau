#include "Spectrogram.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <utility>

namespace hello::kit {

    namespace {

        constexpr double pi = 3.14159265358979323846;

        // Transforms data in place, e^(-2 pi i k n / N), by the iterative radix-2 algorithm of
        // Cooley and Tukey. The size is a power of two.
        void transform(std::vector<std::complex<double>> &data) {
            const auto size = data.size();
            for (size_t i = 1, j = 0; i < size; ++i) {
                auto bit = size >> 1;
                for (; j & bit; bit >>= 1) {
                    j ^= bit;
                }
                j ^= bit;
                if (i < j) {
                    std::swap(data[i], data[j]);
                }
            }
            for (size_t length = 2; length <= size; length <<= 1) {
                const double angle = -2 * pi / double(length);
                const std::complex<double> unit(std::cos(angle), std::sin(angle));
                for (size_t start = 0; start < size; start += length) {
                    std::complex<double> twiddle(1, 0);
                    for (size_t k = 0; k < length / 2; ++k) {
                        const auto even = data[start + k];
                        const auto odd = data[start + k + length / 2] * twiddle;
                        data[start + k] = even + odd;
                        data[start + k + length / 2] = even - odd;
                        twiddle *= unit;
                    }
                }
            }
        }

    }

    Spectrogram::Spectrogram() = default;

    Spectrogram Spectrogram::of(const WaveAudio &audio) {
        Spectrogram result;
        const auto frames = audio.frameCount();
        if (frames <= 0 || audio.sampleRate <= 0) {
            return result;
        }
        result.m_sampleRate = audio.sampleRate;

        // The channels mixed
        std::vector<double> mono(size_t(frames), 0);
        for (qsizetype i = 0; i < frames; ++i) {
            double sum = 0;
            for (int c = 0; c < audio.channels; ++c) {
                sum += audio.samples[size_t(i * audio.channels + c)];
            }
            mono[size_t(i)] = sum / audio.channels;
        }

        // The Hamming window, and the scale that gives a sine of full scale a magnitude of 1
        std::vector<double> window(windowSize);
        double sum = 0;
        for (int n = 0; n < windowSize; ++n) {
            window[size_t(n)] = 0.54 - 0.46 * std::cos(2 * pi * n / windowSize);
            sum += window[size_t(n)];
        }
        const double scale = 2 / sum;

        result.m_frameCount = int(frames / hopSize) + 1;
        result.m_magnitudes.resize(size_t(result.m_frameCount) * binCount);
        std::vector<std::complex<double>> data(windowSize);
        for (int frame = 0; frame < result.m_frameCount; ++frame) {
            const qsizetype first = qsizetype(frame) * hopSize - windowSize / 2;
            for (int n = 0; n < windowSize; ++n) {
                const auto at = first + n;
                const double sample = at >= 0 && at < frames ? mono[size_t(at)] : 0;
                data[size_t(n)] = {sample * window[size_t(n)], 0};
            }
            transform(data);
            auto row = result.m_magnitudes.begin() + ptrdiff_t(frame) * binCount;
            for (int bin = 0; bin < binCount; ++bin) {
                const auto magnitude = float(std::abs(data[size_t(bin)]) * scale);
                row[bin] = magnitude;
                result.m_peak = std::max(result.m_peak, magnitude);
            }
        }
        return result;
    }

    int Spectrogram::sampleRate() const {
        return m_sampleRate;
    }

    int Spectrogram::frameCount() const {
        return m_frameCount;
    }

    double Spectrogram::timeOf(int frame) const {
        return m_sampleRate > 0 ? double(frame) * hopSize * 1000 / m_sampleRate : 0;
    }

    double Spectrogram::frequencyOf(double bin) const {
        return bin * m_sampleRate / windowSize;
    }

    double Spectrogram::binOf(double frequency) const {
        return m_sampleRate > 0 ? frequency * windowSize / m_sampleRate : 0;
    }

    float Spectrogram::magnitude(int frame, int bin) const {
        if (frame < 0 || frame >= m_frameCount || bin < 0 || bin >= binCount) {
            return 0;
        }
        return m_magnitudes[size_t(frame) * binCount + size_t(bin)];
    }

    float Spectrogram::peak() const {
        return m_peak;
    }

}
