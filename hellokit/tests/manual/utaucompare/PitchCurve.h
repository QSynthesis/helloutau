#ifndef UTAUCOMPARE_PITCHCURVE_H
#define UTAUCOMPARE_PITCHCURVE_H

#include <cstdint>
#include <vector>

#include <QtCore/QList>
#include <QtCore/QString>

namespace utaucompare {

    /// The pitch curve passed to a synth tool: one value every five ticks, in cents.
    ///
    /// \note The decoding is reimplemented here rather than taken from stdutau. stdutau is under
    ///       test, and a comparison that encodes and decodes with the code under test always
    ///       agrees with itself.
    QList<int> decodePitch(const QString &encoded);

    /// The per-value distance between two curves, in cents.
    ///
    /// The two sides rarely contain the same number of values, which alone is not an audible
    /// difference: beyond the end of a curve there is no bend, so the missing values of the
    /// shorter side are treated as zero.
    ///
    /// \note This is established, not assumed. UTAU omits trailing zeros, and in a real tuned
    ///       project it does so for half of the notes: one note sends fifteen values of -500
    ///       and ends, while the note continues for another twenty-four. A synth tool that held
    ///       the last value would sing that note five semitones flat until its end, so no
    ///       synth tool holds it, and the omitted values are the trimmed zeros.
    struct CurveDeviation {
        int readings = 0; ///< the number of compared values, the length of the longer curve
        int peak = 0;     ///< the largest single deviation, in cents
        double mean = 0;  ///< the mean over all values, in cents
        int peakAt = -1;  ///< the index of the largest deviation
    };

    /// \param histogram counts every value by its deviation, extended as needed, so that a
    ///        caller can report the median and not only the maximum
    CurveDeviation compareCurves(const QList<int> &ours, const QList<int> &theirs,
                                 std::vector<std::int64_t> *histogram = nullptr);

}

#endif // UTAUCOMPARE_PITCHCURVE_H
