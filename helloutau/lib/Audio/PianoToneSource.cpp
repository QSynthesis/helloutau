#include "PianoToneSource.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace hello::daw {

    namespace {

        constexpr double Pi = 3.14159265358979323846;
        constexpr double Attack = 0.008;
        constexpr double Release = 0.06;
        constexpr double Inharmonicity = 0.0009;
        constexpr double ModeAmplitudes[] = {1.0, 0.52, 0.30, 0.18, 0.11,
                                             0.07, 0.045, 0.03, 0.02, 0.014};

        // A deterministic noise value avoids mutable state in the audio callback.
        double hammerNoise(qsizetype frame) {
            auto value = quint32(frame) * 747796405u + 2891336453u;
            value = ((value >> ((value >> 28u) + 4u)) ^ value) * 277803737u;
            value = (value >> 22u) ^ value;
            return double(value) / double(std::numeric_limits<quint32>::max()) * 2.0 - 1.0;
        }

    }

    PianoToneSource::PianoToneSource(int sampleRate, double frequency, double duration,
                                     double amplitude)
        : m_sampleRate(std::max(1, sampleRate)), m_frequency(std::max(0.0, frequency)),
          m_amplitude(std::max(0.0, amplitude)),
          m_frames(std::max<qsizetype>(0, qsizetype(m_sampleRate * duration))) {
        for (size_t i = 0; i < m_angularFrequencies.size(); ++i) {
            const auto mode = double(i + 1);
            const auto frequencyRatio = mode * (1.0 + Inharmonicity * mode * mode);
            m_angularFrequencies[i] = 2.0 * Pi * m_frequency * frequencyRatio / m_sampleRate;
        }
    }

    PianoToneSource::~PianoToneSource() = default;

    qsizetype PianoToneSource::read(float *out, qsizetype frames, int channels) noexcept {
        const auto position = m_position.load(std::memory_order_relaxed);
        const auto count = std::clamp<qsizetype>(m_frames - position, 0, frames);
        if (channels <= 0) {
            return count;
        }
        for (qsizetype i = 0; i < count; ++i) {
            const auto frame = position + i;
            const auto time = double(frame) / m_sampleRate;
            const auto attack = std::min(1.0, time / Attack);
            const auto release = std::min(1.0, double(m_frames - frame) /
                                                    (m_sampleRate * Release));
            const auto envelope = attack * release;
            double sample = 0;
            for (size_t mode = 0; mode < m_angularFrequencies.size(); ++mode) {
                const auto decay = 1.7 + double(mode) * 0.42;
                sample += ModeAmplitudes[mode] * std::exp(-decay * time) *
                          std::sin(m_angularFrequencies[mode] * frame);
            }
            const auto hammer = 0.16 * hammerNoise(frame) * std::exp(-time / 0.003);
            sample = m_amplitude * envelope * (sample / 2.3 + hammer);
            for (int channel = 0; channel < channels; ++channel) {
                const auto stereo = channels > 1 ? (channel == 0 ? 0.985 : 1.015) : 1.0;
                out[i * channels + channel] = float(sample * stereo);
            }
        }
        m_position.store(position + count, std::memory_order_relaxed);
        return count;
    }

    qint64 PianoToneSource::position() const {
        return m_position.load(std::memory_order_relaxed);
    }

}
