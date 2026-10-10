#include "DeviceClock.h"

#include <algorithm>

namespace hello::daw {

    DeviceClock::DeviceClock(int sampleRate) : m_sampleRate(std::max(1, sampleRate)) {
    }

    void DeviceClock::pulled(qsizetype frames, double before, double after,
                             Clock::time_point now) noexcept {
        const qint64 count = m_count.load(std::memory_order_relaxed);
        if (count == 0) {
            m_start.store(now.time_since_epoch().count(), std::memory_order_relaxed);
            m_initial.store(before, std::memory_order_relaxed);
        }
        m_frames += frames;
        auto &pull = m_pulls[size_t(count % keptPulls)];
        pull.end.store(m_frames, std::memory_order_relaxed);
        pull.after.store(after, std::memory_order_relaxed);
        m_count.store(count + 1, std::memory_order_release);
    }

    std::optional<double> DeviceClock::heard(Clock::time_point now) const {
        const qint64 count = m_count.load(std::memory_order_acquire);
        if (count == 0) {
            return std::nullopt;
        }
        const Clock::time_point start{Clock::duration(m_start.load(std::memory_order_relaxed))};
        // The frame played now, counted from the first frame pulled
        const double played =
            std::chrono::duration<double>(now - start).count() * double(m_sampleRate);
        // Newest first, short of the oldest pulls, which the audio thread may be replacing
        const qint64 oldest = std::max<qint64>(0, count - (keptPulls - 2));
        for (qint64 i = count - 1; i >= oldest; --i) {
            const auto &pull = m_pulls[size_t(i % keptPulls)];
            const auto end = double(pull.end.load(std::memory_order_relaxed));
            const auto after = pull.after.load(std::memory_order_relaxed);
            if (played >= end) {
                return after;
            }
            const auto &previous = m_pulls[size_t((i + keptPulls - 1) % keptPulls)];
            const double begin = i == 0 ? 0 : double(previous.end.load(std::memory_order_relaxed));
            const double before = i == 0 ? m_initial.load(std::memory_order_relaxed)
                                         : previous.after.load(std::memory_order_relaxed);
            if (played >= begin || i == oldest) {
                const double part =
                    end > begin ? std::clamp((played - begin) / (end - begin), 0.0, 1.0) : 1.0;
                return before + part * (after - before);
            }
        }
        return std::nullopt;
    }

}
