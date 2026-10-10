#include "PortamentoSettings.h"

#include <algorithm>
#include <iterator>

namespace hello::kit {

    namespace {

        constexpr const char *modeNames[] = {"preset", "custom", "addPoints"};
        constexpr const char *positionNames[] = {"center", "left", "right"};

        // Returns the index of name in names, or -1.
        template <size_t N>
        int indexOf(const char *const (&names)[N], const QString &name) {
            for (size_t i = 0; i < N; ++i) {
                if (name == QLatin1String(names[i])) {
                    return int(i);
                }
            }
            return -1;
        }

        // Returns the type of the segment of points in which x lies, the type of the first point
        // before it and of the last point after it.
        PortamentoPoint::Type typeAt(const QList<PortamentoPoint> &points, double x) {
            for (const auto &point : points) {
                if (x <= point.x) {
                    return point.type;
                }
            }
            return points.last().type;
        }

        // Returns count points from from to to, evenly apart, on the curve of points.
        QList<PortamentoPoint> spread(const QList<PortamentoPoint> &points, int count, double from,
                                      double to) {
            QList<PortamentoPoint> result;
            result.reserve(count);
            for (int i = 0; i < count; ++i) {
                const double x = from + (to - from) * double(i) / double(count - 1);
                result.push_back({x, PortamentoPoint::heightAt(points, x), typeAt(points, x)});
            }
            return result;
        }

        // Splits the longest segment of points in its middle.
        void splitLongest(QList<PortamentoPoint> &points) {
            qsizetype longest = 1;
            for (qsizetype i = 2; i < points.size(); ++i) {
                if (points.at(i).x - points.at(i - 1).x >
                    points.at(longest).x - points.at(longest - 1).x) {
                    longest = i;
                }
            }
            auto &right = points[longest];
            const double x = (points.at(longest - 1).x + right.x) / 2;
            PortamentoPoint middle{x, PortamentoPoint::heightAt(points, x), right.type};
            // The halves of an S curve are a J curve and an R curve.
            if (right.type == PortamentoPoint::S) {
                middle.type = PortamentoPoint::J;
                right.type = PortamentoPoint::R;
            }
            points.insert(longest, middle);
        }

    }

    QList<PortamentoPoint> PortamentoSettings::pointsFor(const QList<PortamentoPoint> &current,
                                                         double duration) const {
        if (mode == Custom) {
            PortamentoPoint first{double(start), 0, PortamentoPoint::S};
            PortamentoPoint second{double(start + length), 0, PortamentoPoint::S};
            if (current.size() == 2) {
                first.y = current.first().y;
                first.type = current.first().type;
                second.y = current.last().y;
                second.type = current.last().type;
            }
            return {first, second};
        }

        const QList<PortamentoPoint> preset = {
            {position == Right ? 0 : -double(presetLength), 0, PortamentoPoint::S},
            {position == Left ? 0 : double(presetLength),   0, PortamentoPoint::S},
        };
        const int wanted = std::max(2, count);
        if (mode == Preset) {
            return preset;
        }
        if (current.isEmpty()) {
            return spread(preset, wanted, preset.first().x, preset.last().x);
        }
        const double first = current.first().x;
        if (evenlyDistributed) {
            return spread(current, wanted, first, std::max(first, duration));
        }
        if (wanted < current.size()) {
            return spread(current, wanted, first, current.last().x);
        }

        auto result = current;
        const qsizetype added = wanted - current.size();
        const auto last = current.last();
        const double room = duration - last.x;
        if (room > 0) {
            const double interval = std::min(appendedInterval, room / double(added));
            for (qsizetype i = 1; i <= added; ++i) {
                result.push_back({last.x + interval * double(i), last.y, PortamentoPoint::S});
            }
            return result;
        }
        for (qsizetype i = 0; i < added; ++i) {
            if (result.size() < 2) {
                result.push_back(last);
                continue;
            }
            splitLongest(result);
        }
        return result;
    }

    QJsonObject PortamentoSettings::toJson() const {
        return QJsonObject{
            {QLatin1String("mode"),              QLatin1String(modeNames[mode])        },
            {QLatin1String("position"),          QLatin1String(positionNames[position])},
            {QLatin1String("presetLength"),      presetLength                          },
            {QLatin1String("start"),             start                                 },
            {QLatin1String("length"),            length                                },
            {QLatin1String("count"),             count                                 },
            {QLatin1String("evenlyDistributed"), evenlyDistributed                     },
        };
    }

    PortamentoSettings PortamentoSettings::fromJson(const QJsonObject &object) {
        PortamentoSettings settings;
        const int mode = indexOf(modeNames, object.value(QLatin1String("mode")).toString());
        if (mode >= 0) {
            settings.mode = Mode(mode);
        }
        const int position =
            indexOf(positionNames, object.value(QLatin1String("position")).toString());
        if (position >= 0) {
            settings.position = Position(position);
        }
        const int presetLength =
            object.value(QLatin1String("presetLength")).toInt(settings.presetLength);
        if (std::find(std::begin(presetLengths), std::end(presetLengths), presetLength) !=
            std::end(presetLengths)) {
            settings.presetLength = presetLength;
        }
        settings.start = object.value(QLatin1String("start")).toInt(settings.start);
        const int length = object.value(QLatin1String("length")).toInt(settings.length);
        if (length >= 0) {
            settings.length = length;
        }
        const int count = object.value(QLatin1String("count")).toInt(settings.count);
        if (count >= 2) {
            settings.count = count;
        }
        settings.evenlyDistributed =
            object.value(QLatin1String("evenlyDistributed")).toBool(settings.evenlyDistributed);
        return settings;
    }

}
