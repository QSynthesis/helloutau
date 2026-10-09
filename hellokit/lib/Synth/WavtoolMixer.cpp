#include "WavtoolMixer.h"

#include <algorithm>
#include <cmath>

#include <QtCore/QRegularExpression>

#include <hellokit/Document/TempoMap.h>

namespace hello::kit {

    namespace {

        // Milliseconds to samples, rounded to the nearest as wavtool.exe rounds them
        qint64 samplesOf(double milliseconds) {
            return qint64(std::llround(milliseconds * WavtoolMixer::sampleRate / 1000));
        }

        std::optional<double> numberOf(const QString &text) {
            bool ok = false;
            const double value = text.toDouble(&ok);
            return ok && std::isfinite(value) ? std::optional<double>(value) : std::nullopt;
        }

    }

    std::optional<WavtoolCall> WavtoolCall::parse(const QStringList &arguments) {
        // <out> <in> <stp> <length>@<tempo>[+-]<correction> [envelope]. UTAU writes the length
        // of a note with $patch or $direct in milliseconds instead.
        if (arguments.size() < 4) {
            return std::nullopt;
        }
        static const QRegularExpression lengthPattern(
            QStringLiteral(R"(^([0-9.]+)@([0-9.]+)([+-][0-9.]+)?$)"));
        const auto match = lengthPattern.match(arguments[3]);
        const auto stp = numberOf(arguments[2]);
        if (!stp) {
            return std::nullopt;
        }
        std::optional<double> length;
        if (match.hasMatch()) {
            const auto ticks = numberOf(match.captured(1));
            const auto tempo = numberOf(match.captured(2));
            const auto correction = match.captured(3).isEmpty() ? std::optional<double>(0)
                                                                : numberOf(match.captured(3));
            if (ticks && tempo && correction && *tempo > 0) {
                length = TempoMap::duration(*ticks, *tempo) + *correction;
            }
        } else {
            length = numberOf(arguments[3]);
        }
        if (!length) {
            return std::nullopt;
        }

        WavtoolCall call;
        call.startPoint = *stp;
        call.length = *length;

        // A rest passes two zeros instead of an envelope.
        const auto envelope = arguments.mid(4);
        if (envelope.size() < 8) {
            return call;
        }
        std::vector<double> values;
        for (const auto &text : envelope) {
            const auto value = numberOf(text);
            if (!value) {
                return std::nullopt;
            }
            values.push_back(*value);
        }
        call.hasEnvelope = true;
        call.p1 = values[0];
        call.p2 = values[1];
        call.p3 = values[2];
        call.v1 = values[3];
        call.v2 = values[4];
        call.v3 = values[5];
        call.v4 = values[6];
        call.overlap = values[7];
        if (values.size() > 8) {
            call.p4 = values[8];
        }
        if (values.size() > 10) {
            call.p5 = values[9];
            call.v5 = values[10];
        }
        return call;
    }

    QList<WavtoolMixer::Segment> WavtoolMixer::layOut(const QList<WavtoolCall> &calls) {
        QList<Segment> segments;
        segments.reserve(calls.size());
        qint64 end = 0;
        for (int i = 0; i < calls.size(); ++i) {
            const auto &call = calls[i];
            Segment segment;
            if (i == 0) {
                // The first call has nothing to overlap.
                segment.start = 0;
            } else if (call.overlap >= 0) {
                // An overlap longer than the track would read before its start, which
                // wavtool.exe does without a rule; the segment starts at 0 instead.
                segment.start = std::max<qint64>(0, end - samplesOf(call.overlap));
            } else {
                // A negative overlap leaves a gap one sample shorter than its length.
                segment.start = end + std::max<qint64>(0, samplesOf(-call.overlap) - 1);
            }
            // The fragment is read up to the end of the length after the offset, each rounded.
            segment.skip = samplesOf(call.startPoint);
            segment.length =
                std::max<qint64>(0, samplesOf(call.startPoint + call.length) - segment.skip);
            segment.silent = !call.hasEnvelope;
            if (call.hasEnvelope) {
                // Each point is placed in milliseconds on the fragment, after the offset, and
                // rounded there, as the length is; the last two back from the exact length
                // rather than from the rounded one.
                const auto at = [&call, &segment](double milliseconds) {
                    return samplesOf(call.startPoint + milliseconds) - segment.skip;
                };
                auto &points = segment.envelope;
                points.push_back({0, 0});
                points.push_back({at(call.p1), call.v1 / 100});
                points.push_back({at(call.p1 + call.p2), call.v2 / 100});
                if (call.p5 && call.v5) {
                    points.push_back({at(call.p1 + call.p2 + *call.p5), *call.v5 / 100});
                }
                points.push_back({at(call.length - call.p4 - call.p3), call.v3 / 100});
                points.push_back({at(call.length - call.p4), call.v4 / 100});
                points.push_back({segment.length, 0});
            }
            end = segment.start + segment.length;
            segments.push_back(std::move(segment));
        }
        return segments;
    }

    qint64 WavtoolMixer::lengthOf(const QList<Segment> &segments) {
        return segments.isEmpty() ? 0 : segments.last().start + segments.last().length;
    }

    double WavtoolMixer::gainAt(const Segment &segment, qint64 position) {
        const auto &points = segment.envelope;
        if (points.isEmpty()) {
            return 0;
        }
        // From the last point at or before the position toward the next one, so that of several
        // points at one position the last applies there: an envelope whose first two points are
        // both at 0 has the volume of the second from the first sample on.
        int from = -1;
        while (from + 1 < points.size() && points[from + 1].first <= position) {
            ++from;
        }
        if (from < 0) {
            return points.first().second;
        }
        if (from + 1 >= points.size()) {
            return points.last().second;
        }
        const auto &[x0, y0] = points[from];
        const auto &[x1, y1] = points[from + 1];
        return y0 + (y1 - y0) * double(position - x0) / double(x1 - x0);
    }

    void WavtoolMixer::mix(const QList<Segment> &segments, const FragmentGetter &fragment,
                           qint64 first, qint64 count, qint16 *out) {
        std::vector<qint32> sums(size_t(std::max<qint64>(count, 0)), 0);
        const qint64 last = first + count;
        for (int i = 0; i < segments.size(); ++i) {
            const auto &segment = segments[i];
            const qint64 begin = std::max(first, segment.start);
            const qint64 end = std::min(last, segment.start + segment.length);
            if (segment.silent || begin >= end) {
                continue;
            }
            const auto samples = fragment(i);
            if (!samples) {
                continue;
            }
            for (qint64 t = begin; t < end; ++t) {
                const qint64 within = t - segment.start;
                const qint64 source = segment.skip + within;
                if (source < 0 || source >= qint64(samples->size())) {
                    continue;
                }
                // The track sample so far plus the weighted fragment sample, truncated as a whole
                const double sum = double(sums[size_t(t - first)]) +
                                   double((*samples)[size_t(source)]) * gainAt(segment, within);
                sums[size_t(t - first)] = std::clamp(qint32(sum), -32768, 32767);
            }
        }
        for (qint64 t = 0; t < count; ++t) {
            out[t] = qint16(sums[size_t(t)]);
        }
    }

}
