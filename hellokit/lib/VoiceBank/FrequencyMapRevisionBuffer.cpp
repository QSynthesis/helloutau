#include "FrequencyMapRevisionBuffer.h"

#include <algorithm>
#include <cmath>

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
        return !std::equal(m_map.frames.begin(), m_map.frames.end(), m_saved.frames.begin(),
                           m_saved.frames.end(),
                           [](const auto &a, const auto &b) { return a.frequency == b.frequency; });
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
        return true;
    }

    bool FrequencyMapRevisionBuffer::redo() {
        if (!canRedo()) {
            return false;
        }
        const auto &step = m_steps[m_done++];
        write(step.first, step.after);
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
                                           const std::vector<double> &frequencies) {
        std::size_t begin = 0;
        std::size_t end = frequencies.size();
        while (begin < end && m_map.frames[first + begin].frequency == frequencies[begin]) {
            ++begin;
        }
        while (end > begin && m_map.frames[first + end - 1].frequency == frequencies[end - 1]) {
            --end;
        }
        if (begin == end) {
            return false;
        }
        Step step;
        step.first = first + begin;
        for (std::size_t i = begin; i < end; ++i) {
            step.before.push_back(m_map.frames[first + i].frequency);
            step.after.push_back(frequencies[i]);
        }
        m_steps.resize(m_done);
        m_steps.push_back(std::move(step));
        m_done = m_steps.size();
        write(m_steps.back().first, m_steps.back().after);
        return true;
    }

    void FrequencyMapRevisionBuffer::write(std::size_t first,
                                           const std::vector<double> &frequencies) {
        for (std::size_t i = 0; i < frequencies.size(); ++i) {
            m_map.frames[first + i].frequency = frequencies[i];
        }
    }

}
