#ifndef UTAUCOMPARE_SCRIPTCOMPARE_H
#define UTAUCOMPARE_SCRIPTCOMPARE_H

#include <cstdint>
#include <vector>

#include <QtCore/QList>
#include <QtCore/QString>

#include <hellokit/Synth/SynthPlan.h>

#include "PitchCurve.h"
#include "UtauScript.h"

namespace utaucompare {

    /// One argument that differs between the plan of HelloUTAU and the script of UTAU.
    struct ArgumentDifference {
        int noteIndex = 0;
        QString what; ///< \c flags , \c realLength , ... See \c compare() for the names.
        QString ours;
        QString theirs;
    };

    /// The deviation of the pitch curve of one note from that of UTAU.
    struct CurveDifference {
        int noteIndex = 0;
        CurveDeviation deviation;
        int ourReadings = 0;
        int theirReadings = 0;
    };

    /// The comparison result of the two sides.
    struct Comparison {
        int notesCompared = 0;
        /// Every compared pitch value, counted by its deviation in cents. The worst note shows
        /// the maximum deviation, and this histogram shows how typical it is.
        std::vector<std::int64_t> readings;
        /// Arguments that differ in text but not in value, where UTAU rounded a measurement on
        /// output. Counted rather than listed, because they do not affect the render, but a
        /// report that omitted them would conceal how its result was obtained.
        int spelling = 0;
        int onlyOurs = 0;   ///< notes rendered by HelloUTAU without a call in the UTAU script
        int onlyTheirs = 0; ///< calls in the UTAU script without a note in HelloUTAU
        QList<ArgumentDifference> arguments;
        QList<CurveDifference> curves; ///< every note with a curve on either side
    };

    /// Compares the engine arguments of HelloUTAU with those UTAU passed.
    ///
    /// Arguments are identified by name rather than by position, so that a report states
    /// \c flags rather than "resampler argument 5". The pitch curve is excluded from the
    /// argument comparison and compared as a curve instead: the number of values and their
    /// textual form are not audible differences, and a VB6 program and a C++ program do not
    /// agree on the last digit of either.
    ///
    /// \note Two of the arguments, \c cacheFile and \c outputFile , are chosen by the caller of
    ///       the render rather than by the synthesis layer, so they differ unless both runs
    ///       write to the same location. They are nevertheless compared and reported, because
    ///       the two sides name cache files by different schemes. See \c cacheFileFor() in
    ///       SynthPlan.cpp.
    Comparison compare(const QList<hello::kit::SynthStep> &ours, const QList<ScriptCall> &theirs);

}

#endif // UTAUCOMPARE_SCRIPTCOMPARE_H
