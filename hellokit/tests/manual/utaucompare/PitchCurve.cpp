#include "PitchCurve.h"

#include <algorithm>
#include <cmath>

namespace utaucompare {

    namespace {

        // UTAU's own alphabet, which is the usual base64 one. Two characters carry one reading as
        // a twelve bit two's complement number, and #n# after a reading repeats it n more times.
        constexpr const char ALPHABET[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        int valueOf(QChar c) {
            for (int i = 0; i < 64; ++i) {
                if (c == QLatin1Char(ALPHABET[i])) {
                    return i;
                }
            }
            return -1;
        }

    }

    QList<int> decodePitch(const QString &encoded) {
        QList<int> out;
        for (int i = 0; i < encoded.size();) {
            if (encoded.at(i) == QLatin1Char('#')) {
                const int close = encoded.indexOf(QLatin1Char('#'), i + 1);
                if (close < 0 || out.isEmpty()) {
                    return out;
                }
                bool ok = false;
                const int repeats = encoded.mid(i + 1, close - i - 1).toInt(&ok);
                if (!ok) {
                    return out;
                }
                out.reserve(out.size() + repeats);
                for (int n = 0; n < repeats; ++n) {
                    out += out.last();
                }
                i = close + 1;
                continue;
            }
            if (i + 1 >= encoded.size()) {
                return out;
            }
            const int high = valueOf(encoded.at(i));
            const int low = valueOf(encoded.at(i + 1));
            if (high < 0 || low < 0) {
                return out;
            }
            const int raw = high * 64 + low;
            out += raw > 2047 ? raw - 4096 : raw;
            i += 2;
        }
        return out;
    }

    CurveDeviation compareCurves(const QList<int> &ours, const QList<int> &theirs,
                                 std::vector<std::int64_t> *histogram) {
        CurveDeviation out;
        out.readings = int(std::max(ours.size(), theirs.size()));
        if (out.readings == 0) {
            return out;
        }

        const auto at = [](const QList<int> &curve, int i) {
            return i < curve.size() ? curve.at(i) : 0;
        };

        double total = 0;
        for (int i = 0; i < out.readings; ++i) {
            const int difference = std::abs(at(ours, i) - at(theirs, i));
            total += difference;
            if (difference > out.peak) {
                out.peak = difference;
                out.peakAt = i;
            }
            if (histogram) {
                if (int(histogram->size()) <= difference) {
                    histogram->resize(difference + 1, 0);
                }
                (*histogram)[difference]++;
            }
        }
        out.mean = total / out.readings;
        return out;
    }

}
