#include "PitchBend.h"

#include <algorithm>
#include <cmath>

#include <QtCore/QJsonArray>

#include <hellokit/Document/DocumentConstants.h>

#include "Note.h"

namespace hello::kit {

    QJsonObject PitchBend::toJson() const {
        QJsonArray array;
        for (const double value : values) {
            array.append(value);
        }
        QJsonObject object{
            {QLatin1String("values"), array}
        };
        if (start) {
            object.insert(QLatin1String("start"), *start);
        }
        return object;
    }

    PitchBend PitchBend::fromJson(const QJsonObject &object) {
        PitchBend bend;
        const auto start = object.value(QLatin1String("start"));
        if (start.isDouble()) {
            bend.start = start.toDouble();
        }
        for (const auto value : object.value(QLatin1String("values")).toArray()) {
            bend.values.push_back(value.toDouble());
        }
        return bend;
    }

    namespace {

        // The Mode1 values lie this many ticks apart.
        constexpr int BendInterval = 5;

        double ticksOf(double milliseconds, double tempo) {
            return milliseconds * tempo / 60 * ticksPerQuarter / 1000;
        }

        double millisecondsOf(double ticks, double tempo) {
            return ticks / ticksPerQuarter * 1000 * 60 / tempo;
        }

        // Returns the value of bend at tick, or std::nullopt before its first value and after
        // the interval that follows its last value.
        std::optional<double> valueAt(const std::optional<PitchBend> &bend, double tick,
                                      double tempo) {
            if (!bend || bend->values.isEmpty()) {
                return std::nullopt;
            }
            const double position = (tick - ticksOf(bend->start.value_or(0), tempo)) / BendInterval;
            if (position < 0) {
                return std::nullopt;
            }
            const auto &values = bend->values;
            const auto k = qsizetype(std::floor(position));
            if (k >= values.size()) {
                return std::nullopt;
            }
            const double next = values[std::min(k + 1, values.size() - 1)];
            return values[k] + (next - values[k]) * (position - double(k));
        }

    }

    PreviousBend PreviousBend::of(const Note &previous, const Note &note) {
        PreviousBend result;
        result.bend = previous.pitchBend;
        result.length = previous.length;
        if (!previous.isRest() && previous.noteNum > 0) {
            result.offset = (previous.noteNum - note.noteNum) * 100.0;
        }
        return result;
    }

    double PitchBend::curveAt(const std::optional<PitchBend> &bend, const PreviousBend &previous,
                              double tick, double tempo) {
        if (const auto own = valueAt(bend, tick, tempo)) {
            return *own;
        }
        // Before the start of the note the curve of the previous note applies, also after the
        // values of this note end.
        if (tick < 0) {
            if (const auto before = valueAt(previous.bend, tick + previous.length, tempo)) {
                return *before + previous.offset;
            }
        }
        return 0;
    }

    PitchBend PitchBend::drawn(const std::optional<PitchBend> &bend, const PreviousBend &previous,
                               double tempo, double tick, const QList<double> &values) {
        if (values.isEmpty()) {
            return bend.value_or(PitchBend());
        }
        const auto thousandth = [](double milliseconds) {
            return std::round(milliseconds * 1000) / 1000;
        };
        if (!bend || bend->values.isEmpty()) {
            PitchBend result;
            result.start = thousandth(millisecondsOf(tick, tempo));
            result.values = values;
            return result;
        }

        const double start = bend->start.value_or(0);
        const double origin = ticksOf(start, tempo);
        const auto size = bend->values.size();
        const auto first = qsizetype(std::llround((tick - origin) / BendInterval));
        const auto from = std::min<qsizetype>(first, 0);
        const auto to = std::max(size, first + values.size());

        PitchBend result;
        result.start = from < 0
                           ? thousandth(start - millisecondsOf(double(-from) * BendInterval, tempo))
                           : bend->start;
        for (auto k = from; k < to; ++k) {
            if (k >= first && k < first + values.size()) {
                result.values.push_back(values[k - first]);
            } else if (k >= 0 && k < size) {
                result.values.push_back(bend->values[k]);
            } else {
                result.values.push_back(
                    std::round(curveAt(bend, previous, origin + double(k) * BendInterval, tempo)));
            }
        }
        return result;
    }

}
