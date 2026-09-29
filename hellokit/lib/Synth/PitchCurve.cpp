#include "PitchCurve.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <stdutau/utautils.h>

namespace hello::kit {

    namespace {

        // The value of pi in stdutau, kept so that the values agree to the cent
        constexpr double Pi = 3.1415926;

        // The shortest vibrato in milliseconds that UTAU draws on its own note; see
        // SHORTEST_VIBRATO in stdutau synth.cpp for the measurements.
        constexpr double ShortestVibrato = 50;

        bool isRest(const QString &lyric) {
            const auto bytes = lyric.toUtf8();
            return utau::isRestLyric(std::string(bytes.constData(), size_t(bytes.size())));
        }

        // The shapes of the segment that ends at a point, truncated to whole cents as stdutau
        // does before it adds the vibrato
        double shapeOf(PortamentoPoint::Type type, double x1, double y1, double x2, double y2,
                       double x) {
            if (x1 == x2) {
                return int(y1);
            }
            switch (type) {
                case PortamentoPoint::Linear:
                    return int((y2 - y1) / (x2 - x1) * (x - x1) + y1);
                case PortamentoPoint::J:
                    return int((y1 - y2) * std::cos(Pi / 2 / (x2 - x1) * (x - x1)) + y2);
                case PortamentoPoint::R:
                    return int((y2 - y1) * std::cos(Pi / 2 / (x2 - x1) * (x - x2)) + y1);
                case PortamentoPoint::S:
                    break;
            }
            return int((y1 - y2) / 2 * std::cos(Pi * (x - x1) / (x2 - x1)) + (y1 + y2) / 2);
        }

    }

    PitchCurve::PitchCurve(const QList<Note> &notes, qsizetype index, double tempo)
        : m_tempo(tempo) {
        const auto partOf = [](const Note &note) {
            Part part;
            for (const auto &point : note.portamento) {
                part.points.push_back(
                    {point.x, PortamentoPoint::tenthsFromCents(point.y), point.type});
            }
            part.vibrato = note.vibrato;
            part.length = note.length;
            return part;
        };
        // After a note that is not a rest, the first point starts at its pitch.
        const auto correct = [](const Note *previous, const Note &note, Point &first) {
            if (previous && !isRest(previous->lyric)) {
                first.y =
                    previous->noteNum <= 0 ? 0 : double((previous->noteNum - note.noteNum) * 10);
            }
        };

        const auto &note = notes.at(index);
        const Note *previous = index > 0 ? &notes.at(index - 1) : nullptr;
        m_bend = note.pitchBend;
        if (previous) {
            m_previousBend = previous->pitchBend;
            m_previousLength = previous->length;
            m_previous = partOf(*previous);
            if (!m_previous.points.isEmpty() && index > 1) {
                correct(&notes.at(index - 2), *previous, m_previous.points.first());
            }
        }

        m_current = partOf(note);
        if (m_current.points.isEmpty()) {
            // Without points, a note bends from the previous note at its start.
            Point first;
            correct(previous, note, first);
            m_current.points = {first, Point()};
        } else {
            correct(previous, note, m_current.points.first());
        }

        if (index + 1 < notes.size()) {
            const auto &next = notes.at(index + 1);
            m_next = partOf(next);
            if (!m_next->points.isEmpty()) {
                correct(&note, next, m_next->points.first());
            }
        }
    }

    double PitchCurve::portamentoAt(double tick) const {
        const auto previous = previousAt(tick);
        const auto current = currentAt(tick);
        const auto next = nextAt(tick, tick);
        return current.portamento + previous.portamento + next.portamento + next.shift;
    }

    double PitchCurve::vibratoAt(double tick) const {
        return currentAt(tick).vibrato + previousAt(tick).vibrato + nextAt(tick, tick).vibrato;
    }

    QList<int> PitchCurve::values(const Timing &timing) const {
        // In the order of stdutau: each note's portamento and vibrato first, then the note,
        // the previous one and the next one, so that the sums agree to the last bit
        const auto sumOf = [](const Impact &impact) {
            return impact.portamento + impact.vibrato + impact.shift;
        };
        QList<int> result;
        for (const double tick : readingTicks(timing)) {
            const double sum =
                sumOf(currentAt(tick)) + sumOf(previousAt(tick)) + sumOf(nextAt(tick, tick - 5));
            result.push_back(int(std::floor(sum + 0.5)));
        }
        return result;
    }

    double PitchCurve::mode1At(double tick) const {
        if (const auto own = bendAt(m_bend, tick)) {
            return *own;
        }
        if (tick < 0) {
            if (const auto previous = bendAt(m_previousBend, tick + m_previousLength)) {
                return *previous;
            }
        }
        return 0;
    }

    QList<int> PitchCurve::mode1Values(const Timing &timing) const {
        QList<int> result;
        for (const double tick : readingTicks(timing)) {
            result.push_back(int(std::round(mode1At(tick))));
        }
        return result;
    }

    double PitchCurve::ticksOf(double milliseconds) const {
        return milliseconds * m_tempo / 60 * 480 / 1000;
    }

    QList<double> PitchCurve::readingTicks(const Timing &timing) const {
        const double end =
            double(m_current.length) + ticksOf(-timing.nextPreUtterance + timing.nextOverlap);
        QList<double> ticks;
        for (double tick = ticksOf(-(timing.preUtterance + timing.startPoint)); tick < end + 4;
             tick = tick + 5) {
            ticks.push_back(tick);
        }
        return ticks;
    }

    std::optional<double> PitchCurve::bendAt(const std::optional<PitchBend> &bend,
                                             double tick) const {
        if (!bend || bend->values.isEmpty()) {
            return std::nullopt;
        }
        const double position = (tick - ticksOf(bend->start.value_or(0))) / 5;
        if (position < 0) {
            return std::nullopt;
        }
        const auto &values = bend->values;
        const auto k = qsizetype(std::floor(position));
        if (k >= values.size()) {
            return 0.0;
        }
        const double next = values[std::min(k + 1, values.size() - 1)];
        return values[k] + (next - values[k]) * (position - double(k));
    }

    PitchCurve::Impact PitchCurve::impactOf(const Part &part, double tick, Whose whose) const {
        Impact impact;

        // The segment that contains the tick; before the first point its height, after the
        // last point nothing
        const auto &points = part.points;
        if (points.size() >= 2) {
            if (tick < ticksOf(points.first().x)) {
                impact.portamento = points.first().y * 10;
            } else {
                for (qsizetype i = 0; i + 1 < points.size(); ++i) {
                    const double x2 = ticksOf(points[i + 1].x);
                    if (tick > x2) {
                        continue;
                    }
                    impact.portamento = shapeOf(points[i + 1].type, ticksOf(points[i].x),
                                                points[i].y * 10, x2, points[i + 1].y * 10, tick);
                    break;
                }
            }
        }

        if (!part.vibrato || part.length <= 0) {
            return impact;
        }
        const auto &vibrato = *part.vibrato;
        const double length = part.length;
        const double proportion = vibrato.length / 100.0;
        const bool longEnough =
            m_tempo > 0 &&
            vibrato.length / 100.0 * length * 60000 / (m_tempo * 480) > ShortestVibrato;
        if (!longEnough && whose == Own) {
            return impact;
        }

        const double period = ticksOf(vibrato.period);
        const double span = proportion * length;
        const double start = (1 - proportion) * length;
        const double frequency = 1 / period * 2 * Pi;
        const double phase = vibrato.phase / 100.0 * 2 * Pi;
        const double fadeIn = vibrato.attack / 100.0 * span;
        const double fadeOut = (1 - vibrato.release / 100.0) * span;

        const double x = tick - start;
        double y = vibrato.amplitude * std::sin(frequency * x - phase);
        if (x > 0 && x < span) {
            y += vibrato.offset / 100.0 * vibrato.amplitude;
            // Fading in and out exclude each other, and a short vibrato reaching into a
            // neighbour does not fade; see find_impact() in stdutau synth.cpp.
            double ratio = 1;
            if (!longEnough) {
                ratio = 1;
            } else if (x < fadeIn) {
                ratio = x / fadeIn;
            } else if (x > fadeOut) {
                ratio = 1 - (x - fadeOut) / (span - fadeOut);
            }
            impact.vibrato = ratio * y;
        }
        return impact;
    }

    PitchCurve::Impact PitchCurve::previousAt(double tick) const {
        if (tick > 0) {
            return {};
        }
        return impactOf(m_previous, tick + m_previous.length, Neighbour);
    }

    PitchCurve::Impact PitchCurve::currentAt(double tick) const {
        return impactOf(m_current, tick, Own);
    }

    PitchCurve::Impact PitchCurve::nextAt(double tick, double previousTick) const {
        const double start = nextStart();
        if (tick < start) {
            return {};
        }
        // stdutau stops reading the next note, its vibrato included, once the value before
        // passed its last point: its search for the segment keeps its position from value to
        // value and is not called again after reaching the end.
        const auto &points = m_next->points;
        const bool reading =
            previousTick < start || previousTick - m_current.length <= ticksOf(points.last().x);
        Impact impact;
        if (points.size() > 1 && reading) {
            impact = impactOf(*m_next, tick - m_current.length, Neighbour);
        }
        // Relative to this note rather than to the next one
        impact.shift = -(m_next->points.first().y * 10);
        return impact;
    }

    double PitchCurve::nextStart() const {
        if (!m_next || m_next->points.isEmpty()) {
            return std::numeric_limits<double>::infinity();
        }
        return m_current.length + ticksOf(m_next->points.first().x);
    }

}
