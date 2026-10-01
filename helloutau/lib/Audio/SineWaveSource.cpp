#include "SineWaveSource.h"

#include <algorithm>
#include <cmath>

namespace hello::daw {

    namespace {

        constexpr double Pi = 3.14159265358979323846;
        constexpr double FadeIn = 0.01;
        constexpr double FadeOut = 0.03;

    }

    SineWaveSource::SineWaveSource(int sampleRate, double frequency, double duration,
                                   double amplitude)
        : m_sampleRate(std::max(1, sampleRate)), m_frequency(std::max(0.0, frequency)),
          m_amplitude(amplitude),
          m_frames(std::max<qsizetype>(0, qsizetype(sampleRate * duration))) {
    }

    SineWaveSource::~SineWaveSource() = default;

    qsizetype SineWaveSource::read(float *out, qsizetype frames, int channels) noexcept {
        const auto position = m_position.load(std::memory_order_relaxed);
        const auto count = std::clamp<qsizetype>(m_frames - position, 0, frames);
        for (qsizetype i = 0; i < count; ++i) {
            const auto frame = position + i;
            const double fade = std::min({1.0, frame / (m_sampleRate * FadeIn),
                                          double(m_frames - frame) / (m_sampleRate * FadeOut)});
            const float sample =
                float(m_amplitude * fade * std::sin(2 * Pi * m_frequency * frame / m_sampleRate));
            for (int channel = 0; channel < channels; ++channel) {
                out[i * channels + channel] = sample;
            }
        }
        m_position.store(position + count, std::memory_order_relaxed);
        return count;
    }

    qint64 SineWaveSource::position() const {
        return m_position.load(std::memory_order_relaxed);
    }

}
