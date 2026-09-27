#include "TempoMap.h"

#include <algorithm>

#include "DocumentConstants.h"

namespace hello::kit {

    TempoMap::TempoMap(double initialTempo)
        : m_initialTempo(initialTempo > 0 ? initialTempo : utau::DEFAULT_VALUE_TEMPO) {
        m_startTicks.push_back(0);
        m_startTimes.push_back(0);
    }

    TempoMap TempoMap::of(const Project &project) {
        TempoMap map(project.settings.tempo);
        if (!project.tracks.isEmpty()) {
            const auto &notes = project.tracks.first().notes;
            for (const auto &note : notes) {
                map.append(note.length, note.tempo);
            }
        }
        return map;
    }

    void TempoMap::append(int length, std::optional<double> tempo) {
        const double effective = tempo && *tempo > 0 ? *tempo : tempoAfterEnd();
        const int ticks = std::max(length, 0);
        m_tempos.push_back(effective);
        m_startTicks.push_back(m_startTicks.last() + ticks);
        m_startTimes.push_back(m_startTimes.last() + duration(ticks, effective));
    }

    int TempoMap::noteCount() const {
        return int(m_tempos.size());
    }

    qint64 TempoMap::startTick(int index) const {
        return m_startTicks.at(index);
    }

    double TempoMap::startTime(int index) const {
        return m_startTimes.at(index);
    }

    double TempoMap::tempo(int index) const {
        return m_tempos.at(index);
    }

    int TempoMap::noteAt(double tick) const {
        if (tick < 0) {
            return -1;
        }
        // The last start not after the tick. Of several equal starts, which are notes of zero
        // length, the last is the note that contains the tick.
        const auto it = std::upper_bound(m_startTicks.begin(), m_startTicks.end(), tick);
        return int(it - m_startTicks.begin()) - 1;
    }

    double TempoMap::timeOf(double tick) const {
        if (tick < 0) {
            return duration(tick, m_initialTempo);
        }
        const int index = noteAt(tick);
        if (index >= noteCount()) {
            return m_startTimes.last() +
                   duration(tick - double(m_startTicks.last()), tempoAfterEnd());
        }
        return m_startTimes.at(index) +
               duration(tick - double(m_startTicks.at(index)), m_tempos.at(index));
    }

    double TempoMap::tickOf(double time) const {
        if (time < 0) {
            return time * m_initialTempo * ticksPerQuarter / 60000;
        }
        const auto it = std::upper_bound(m_startTimes.begin(), m_startTimes.end(), time);
        const int index = int(it - m_startTimes.begin()) - 1;
        const double tempo = index >= noteCount() ? tempoAfterEnd() : m_tempos.at(index);
        return double(m_startTicks.at(index)) +
               (time - m_startTimes.at(index)) * tempo * ticksPerQuarter / 60000;
    }

    double TempoMap::duration(double ticks, double tempo) {
        return ticks * 60000 / (tempo * ticksPerQuarter);
    }

    double TempoMap::tempoAfterEnd() const {
        return m_tempos.isEmpty() ? m_initialTempo : m_tempos.last();
    }

}
