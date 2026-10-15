#include "FrequencyMapRevisionBuffer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace hello::kit {

    FrequencyMapRevisionBuffer::FrequencyMapRevisionBuffer(FrequencyMap map)
        : m_map(std::move(map)), m_saved(m_map) {
    }

    const FrequencyMap &FrequencyMapRevisionBuffer::map() const {
        return m_map;
    }

    const FrequencyMap &FrequencyMapRevisionBuffer::savedMap() const {
        return m_saved;
    }

    bool FrequencyMapRevisionBuffer::isModified() const {
        return m_map.averageFrequency != m_saved.averageFrequency ||
               !std::equal(m_map.frames.begin(), m_map.frames.end(), m_saved.frames.begin(),
                           m_saved.frames.end(),
                           [](const auto &a, const auto &b) { return a.frequency == b.frequency; });
    }

    double FrequencyMapRevisionBuffer::key() const {
        return m_map.averageFrequency ? *m_map.averageFrequency : m_map.pitchAverage();
    }

    void FrequencyMapRevisionBuffer::markSaved() {
        m_saved = m_map;
    }

    void FrequencyMapRevisionBuffer::beginStroke(std::size_t frame, double frequency) {
        if (m_stroke) {
            endStroke();
        }
        Stroke stroke;
        stroke.frame = frame;
        stroke.frequency = limited(frequency);
        stroke.before.reserve(m_map.frames.size());
        for (const auto &each : m_map.frames) {
            stroke.before.push_back(each.frequency);
        }
        if (frame < m_map.frames.size()) {
            m_map.frames[frame].frequency = stroke.frequency;
        }
        m_stroke = std::move(stroke);
    }

    void FrequencyMapRevisionBuffer::strokeTo(std::size_t frame, double frequency) {
        if (!m_stroke) {
            return;
        }
        const double from = m_stroke->frequency;
        const double to = limited(frequency);
        const std::size_t start = m_stroke->frame;
        const std::size_t distance = frame > start ? frame - start : start - frame;
        for (std::size_t k = 0; k <= distance; ++k) {
            const std::size_t index = frame > start ? start + k : start - k;
            if (index >= m_map.frames.size()) {
                continue;
            }
            // The points exactly, and the frames between them on the pitch scale, as a line drawn
            // over the semitones of the view
            double value = 0;
            if (from > 0 && to > 0) {
                if (k == distance || from == to) {
                    value = to;
                } else if (k == 0) {
                    value = from;
                } else {
                    const double t = double(k) / double(distance);
                    value = std::exp(std::log(from) + (std::log(to) - std::log(from)) * t);
                }
            }
            m_map.frames[index].frequency = value;
        }
        m_stroke->frame = frame;
        m_stroke->frequency = to;
    }

    bool FrequencyMapRevisionBuffer::endStroke() {
        if (!m_stroke) {
            return false;
        }
        auto before = std::move(m_stroke->before);
        m_stroke.reset();
        std::vector<double> after;
        after.reserve(m_map.frames.size());
        for (const auto &each : m_map.frames) {
            after.push_back(each.frequency);
        }
        // The step is recorded from the frequencies before the stroke.
        write(0, before);
        return apply(0, after);
    }

    void FrequencyMapRevisionBuffer::cancelStroke() {
        if (!m_stroke) {
            return;
        }
        write(0, m_stroke->before);
        m_stroke.reset();
    }

    bool FrequencyMapRevisionBuffer::isStroking() const {
        return m_stroke.has_value();
    }

    bool FrequencyMapRevisionBuffer::scale(std::size_t first, std::size_t count, double factor) {
        if (m_stroke) {
            return false;
        }
        const auto [start, size] = rangeOf(first, count);
        std::vector<double> frequencies;
        frequencies.reserve(size);
        for (std::size_t i = start; i < start + size; ++i) {
            const double frequency = m_map.frames[i].frequency;
            frequencies.push_back(frequency > lowestFrequency ? limited(frequency * factor)
                                                              : frequency);
        }
        return apply(start, frequencies);
    }

    bool FrequencyMapRevisionBuffer::unvoice(std::size_t first, std::size_t count) {
        if (m_stroke) {
            return false;
        }
        const auto [start, size] = rangeOf(first, count);
        return apply(start, std::vector<double>(size, 0));
    }

    bool FrequencyMapRevisionBuffer::interpolate(std::size_t first, std::size_t count) {
        if (m_stroke) {
            return false;
        }
        const auto [start, size] = rangeOf(first, count);
        if (size < 3) {
            return false;
        }
        const double from = m_map.frames[start].frequency;
        const double to = m_map.frames[start + size - 1].frequency;
        if (!(from > lowestFrequency) || !(to > lowestFrequency)) {
            return false;
        }
        std::vector<double> frequencies;
        frequencies.reserve(size - 2);
        const double step = (to - from) / double(size - 1);
        for (std::size_t k = 1; k + 1 < size; ++k) {
            frequencies.push_back(limited(from + step * double(k)));
        }
        return apply(start + 1, frequencies);
    }

    bool FrequencyMapRevisionBuffer::revert(std::size_t first, std::size_t count) {
        if (m_stroke) {
            return false;
        }
        const auto [start, size] = rangeOf(first, count);
        std::vector<double> frequencies;
        frequencies.reserve(size);
        for (std::size_t i = start; i < start + size; ++i) {
            frequencies.push_back(m_saved.frames[i].frequency);
        }
        return apply(start, frequencies);
    }

    bool FrequencyMapRevisionBuffer::revertAll() {
        if (m_stroke) {
            return false;
        }
        std::vector<double> frequencies;
        frequencies.reserve(m_saved.frames.size());
        for (const auto &frame : m_saved.frames) {
            frequencies.push_back(frame.frequency);
        }
        return apply(0, frequencies, m_saved.averageFrequency);
    }

    bool FrequencyMapRevisionBuffer::setFrequency(std::size_t frame, double frequency) {
        if (m_stroke || frame >= m_map.frames.size()) {
            return false;
        }
        return apply(frame, {limited(frequency)});
    }

    bool FrequencyMapRevisionBuffer::setKey(double frequency) {
        if (m_stroke || !m_map.averageFrequency) {
            return false;
        }
        // NaN as well becomes the lowest frequency, as a key not above it does in frqeditor.
        const double key =
            frequency > lowestFrequency ? std::min(frequency, highestFrequency) : lowestFrequency;
        return apply(0, {}, std::optional<double>(key));
    }

    bool FrequencyMapRevisionBuffer::setKeyToMean(std::size_t first, std::size_t count) {
        const auto [start, size] = rangeOf(first, count);
        double sum = 0;
        std::size_t voiced = 0;
        for (std::size_t i = start; i < start + size; ++i) {
            if (m_map.frames[i].frequency > lowestFrequency) {
                sum += m_map.frames[i].frequency;
                ++voiced;
            }
        }
        return voiced > 0 && setKey(sum / double(voiced));
    }

    bool FrequencyMapRevisionBuffer::adoptCorrections(
        std::size_t first, const std::vector<FrequencyMapCheck::Correction> &corrections) {
        if (m_stroke || first >= m_map.frames.size()) {
            return false;
        }
        const std::size_t count = std::min(corrections.size(), m_map.frames.size() - first);
        std::vector<double> frequencies;
        frequencies.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            const auto &correction = corrections[i];
            frequencies.push_back(correction.frequency > 0 ? limited(correction.frequency)
                                                           : m_map.frames[first + i].frequency);
        }
        std::optional<std::optional<double>> key;
        if (m_map.averageFrequency) {
            // The mean in whole cents, as frqeditor computes the key after the adoption: each
            // note is truncated to a cent, and the sum of a 32-bit unsigned register is divided
            // without sign.
            std::uint32_t sum = 0;
            std::uint32_t voiced = 0;
            for (std::size_t i = 0; i < m_map.frames.size(); ++i) {
                const double frequency = i >= first && i < first + count
                                             ? frequencies[i - first]
                                             : m_map.frames[i].frequency;
                if (frequency > lowestFrequency) {
                    const double note = std::log(frequency / 440.0) * 12.0 / std::log(2.0) + 69.0;
                    sum += std::uint32_t(int(note * 100.0));
                    ++voiced;
                }
            }
            const double note = voiced > 0 ? double(int(sum / voiced)) / 100.0 : 60.0;
            key = std::optional<double>(std::pow(2.0, (note - 69.0) / 12.0) * 440.0);
        }
        return apply(first, frequencies, key);
    }

    bool FrequencyMapRevisionBuffer::flatten(double frequency) {
        if (m_stroke) {
            return false;
        }
        std::optional<std::optional<double>> key;
        if (m_map.averageFrequency) {
            key = std::optional<double>(frequency > lowestFrequency
                                            ? std::min(frequency, highestFrequency)
                                            : lowestFrequency);
        }
        return apply(0, std::vector<double>(m_map.frames.size(), limited(frequency)), key);
    }

    bool FrequencyMapRevisionBuffer::canUndo() const {
        return !m_stroke && m_done > 0;
    }

    bool FrequencyMapRevisionBuffer::canRedo() const {
        return !m_stroke && m_done < m_steps.size();
    }

    bool FrequencyMapRevisionBuffer::undo() {
        if (!canUndo()) {
            return false;
        }
        const auto &step = m_steps[--m_done];
        write(step.first, step.before);
        if (step.key) {
            m_map.averageFrequency = step.key->first;
        }
        return true;
    }

    bool FrequencyMapRevisionBuffer::redo() {
        if (!canRedo()) {
            return false;
        }
        const auto &step = m_steps[m_done++];
        write(step.first, step.after);
        if (step.key) {
            m_map.averageFrequency = step.key->second;
        }
        return true;
    }

    double FrequencyMapRevisionBuffer::limited(double frequency) {
        // Also NaN is unvoiced.
        if (!(frequency > lowestFrequency)) {
            return 0;
        }
        return std::min(frequency, highestFrequency);
    }

    std::pair<std::size_t, std::size_t>
        FrequencyMapRevisionBuffer::rangeOf(std::size_t first, std::size_t count) const {
        const std::size_t size = m_map.frames.size();
        if (first >= size) {
            return {size, 0};
        }
        return {first, std::min(count, size - first)};
    }

    bool FrequencyMapRevisionBuffer::apply(std::size_t first,
                                           const std::vector<double> &frequencies,
                                           std::optional<std::optional<double>> key) {
        std::size_t begin = 0;
        std::size_t end = frequencies.size();
        while (begin < end && m_map.frames[first + begin].frequency == frequencies[begin]) {
            ++begin;
        }
        while (end > begin && m_map.frames[first + end - 1].frequency == frequencies[end - 1]) {
            --end;
        }
        const bool keyChanges = key && *key != m_map.averageFrequency;
        if (begin == end && !keyChanges) {
            return false;
        }
        Step step;
        step.first = first + begin;
        for (std::size_t i = begin; i < end; ++i) {
            step.before.push_back(m_map.frames[first + i].frequency);
            step.after.push_back(frequencies[i]);
        }
        if (keyChanges) {
            step.key = std::pair(m_map.averageFrequency, *key);
        }
        m_steps.resize(m_done);
        m_steps.push_back(std::move(step));
        m_done = m_steps.size();
        const auto &done = m_steps.back();
        write(done.first, done.after);
        if (done.key) {
            m_map.averageFrequency = done.key->second;
        }
        return true;
    }

    void FrequencyMapRevisionBuffer::write(std::size_t first,
                                           const std::vector<double> &frequencies) {
        for (std::size_t i = 0; i < frequencies.size(); ++i) {
            m_map.frames[first + i].frequency = frequencies[i];
        }
    }

}
