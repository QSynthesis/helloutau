#include "SineWaveSource.h"

#include <algorithm>
#include <cmath>

#include <QtCore/QtMath>

namespace hello::daw {

    namespace {

        // The linear fades, in seconds
        constexpr double fadeIn = 0.01;
        constexpr double fadeOut = 0.03;

    }

    SineWaveSource::SineWaveSource(int sampleRate, double frequency, double duration,
                                   double amplitude)
        : m_sampleRate(std::max(1, sampleRate)),
          m_angularFrequency(2.0 * M_PI * std::max(0.0, frequency) / m_sampleRate),
          m_amplitude(std::max(0.0, amplitude)),
          m_frames(std::max<qsizetype>(0, qsizetype(m_sampleRate * duration))) {
    }

    SineWaveSource::~SineWaveSource() = default;

    qsizetype SineWaveSource::read(float *out, qsizetype frames, int channels) noexcept {
        const auto position = m_position.load(std::memory_order_relaxed);
        const auto count = std::clamp<qsizetype>(m_frames - position, 0, frames);
        for (qsizetype i = 0; i < count; ++i) {
            const auto frame = position + i;
            const double fade = std::min({1.0, frame / (m_sampleRate * fadeIn),
                                          double(m_frames - frame) / (m_sampleRate * fadeOut)});
            const float sample = float(m_amplitude * fade * std::sin(m_angularFrequency * frame));
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
