#ifndef UTAUCOMPARE_PITCHCURVE_H
#define UTAUCOMPARE_PITCHCURVE_H

#include <cstdint>
#include <vector>

#include <QtCore/QList>
#include <QtCore/QString>

namespace utaucompare {

    /// The pitch curve an engine is handed: one reading every five ticks, in cents.
    ///
    /// \note The decoding here is written out again rather than taken from stdutau. stdutau is
    ///       what is being compared, and a comparison that encodes and decodes with the code
    ///       under test agrees with itself no matter what it does.
    QList<int> decodePitch(const QString &encoded);

    /// How far apart two curves are, reading by reading, in cents.
    ///
    /// The two sides rarely hold the same number of readings, and that on its own is not a
    /// difference anybody can hear: an engine that runs off the end of the curve holds the last
    /// reading, so the short side is compared as though it did.
    struct CurveDeviation {
        int readings = 0; ///< how many were compared, which is the longer of the two
        int peak = 0;     ///< the worst single reading, in cents
        double mean = 0;  ///< over all the readings, in cents
        int peakAt = -1;  ///< which reading the peak is at
    };

    /// \param histogram counts every reading by how far off it was, grown as needed, so that a
    ///        caller can say what the middle reading looks like and not only the worst one
    CurveDeviation compareCurves(const QList<int> &ours, const QList<int> &theirs,
                                 std::vector<std::int64_t> *histogram = nullptr);

}

#endif // UTAUCOMPARE_PITCHCURVE_H
