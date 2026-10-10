#include "PianoToneSource.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QtCore/QtMath>

namespace hello::daw {

    namespace {

        // The envelope, in seconds
        constexpr double attackTime = 0.008;
        constexpr double releaseTime = 0.06;

        constexpr double inharmonicity = 0.0009;
        constexpr double modeAmplitudes[] = {1.0,  0.52,  0.30, 0.18, 0.11,
                                             0.07, 0.045, 0.03, 0.02, 0.014};

        // The sum of modeAmplitudes, by which the modes are normalized
        constexpr double modeAmplitudeSum = [] {
            double sum = 0;
            for (const auto amplitude : modeAmplitudes) {
                sum += amplitude;
            }
            return sum;
        }();

        // The decay rate of the mode of index k, per second, is baseDecay + k * decayStep
        constexpr double baseDecay = 1.7;
        constexpr double decayStep = 0.42;

        // The hammer transient, relative to the normalized modes, and its time constant in
        // seconds
        constexpr double hammerLevel = 0.16;
        constexpr double hammerTime = 0.003;

        // Returns a noise value from -1 to 1 that depends only on \a frame, so that the audio
        // callback keeps no generator state.
        double hammerNoise(qsizetype frame) {
            auto value = quint32(frame) * 747796405u + 2891336453u;
            value = ((value >> ((value >> 28u) + 4u)) ^ value) * 277803737u;
            value = (value >> 22u) ^ value;
            return double(value) / double(std::numeric_limits<quint32>::max()) * 2.0 - 1.0;
        }

    }

    PianoToneSource::PianoToneSource(int sampleRate, double frequency, double duration,
                                     double amplitude)
        : m_sampleRate(std::max(1, sampleRate)), m_amplitude(std::max(0.0, amplitude)),
          m_frames(std::max<qsizetype>(0, qsizetype(m_sampleRate * duration))) {
        const auto fundamental = std::max(0.0, frequency);
        for (size_t i = 0; i < m_angularFrequencies.size(); ++i) {
            const auto mode = double(i + 1);
            const auto frequencyRatio = mode * (1.0 + inharmonicity * mode * mode);
            const auto angularFrequency = 2.0 * M_PI * fundamental * frequencyRatio / m_sampleRate;
            // A mode at or above the Nyquist frequency would alias to an inharmonic tone. The
            // frequency increases with the index, and therefore every later mode is above it too.
            if (angularFrequency >= M_PI) {
                break;
            }
            m_angularFrequencies[i] = angularFrequency;
            ++m_modeCount;
        }
    }

    PianoToneSource::~PianoToneSource() = default;

    qsizetype PianoToneSource::read(float *out, qsizetype frames, int channels) noexcept {
        const auto position = m_position.load(std::memory_order_relaxed);
        const auto count = std::clamp<qsizetype>(m_frames - position, 0, frames);
        for (qsizetype i = 0; i < count; ++i) {
            const auto frame = position + i;
            const auto time = double(frame) / m_sampleRate;
            const auto attack = std::min(1.0, time / attackTime);
            const auto release =
                std::min(1.0, double(m_frames - frame) / (m_sampleRate * releaseTime));
            const auto envelope = attack * release;
            double sample = 0;
            for (size_t mode = 0; mode < m_modeCount; ++mode) {
                const auto decay = baseDecay + double(mode) * decayStep;
                sample += modeAmplitudes[mode] * std::exp(-decay * time) *
                          std::sin(m_angularFrequencies[mode] * frame);
            }
            const auto hammer = hammerLevel * hammerNoise(frame) * std::exp(-time / hammerTime);
            const auto value = float(m_amplitude * envelope * (sample / modeAmplitudeSum + hammer));
            for (int channel = 0; channel < channels; ++channel) {
                out[i * channels + channel] = value;
            }
        }
        m_position.store(position + count, std::memory_order_relaxed);
        return count;
    }

    qint64 PianoToneSource::position() const {
        return m_position.load(std::memory_order_relaxed);
    }

}
