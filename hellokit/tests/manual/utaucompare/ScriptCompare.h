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

    /// One argument our plan and UTAU's script do not agree on.
    struct ArgumentDifference {
        int noteIndex = 0;
        QString what; ///< \c flags , \c realLength , ... See \c compare() on the names.
        QString ours;
        QString theirs;
    };

    /// How far one note's pitch curve is from UTAU's.
    struct CurveDifference {
        int noteIndex = 0;
        CurveDeviation deviation;
        int ourReadings = 0;
        int theirReadings = 0;
    };

    /// What the two sides do and do not agree on.
    struct Comparison {
        int notesCompared = 0;
        /// Every pitch reading compared, counted by how many cents it was off. The worst note
        /// says how bad it can get; this says how typical that is.
        std::vector<std::int64_t> readings;
        /// Arguments that read differently and mean the same, which is a measurement UTAU
        /// rounded where it printed it. Counted rather than listed: it is not a difference in
        /// the render, but a report that never mentions it hides how it was reached.
        int spelling = 0;
        int onlyOurs = 0;   ///< notes we render that UTAU's script has no call for
        int onlyTheirs = 0; ///< calls in the script we have no note for
        QList<ArgumentDifference> arguments;
        QList<CurveDifference> curves; ///< every note that has a curve on either side
    };

    /// Compares what we would hand the engines against what UTAU handed them.
    ///
    /// Arguments are named rather than numbered, so that a report says \c flags rather than
    /// "resampler argument 5". The pitch curve is left out of the argument comparison and
    /// compared as a curve instead: how many readings it holds and how the numbers are spelled
    /// are not differences anybody can hear, and a VB6 program and a C++ one will not agree on
    /// the last digit of either.
    ///
    /// \note Two of the names, \c cacheFile and \c outputFile, are chosen by whoever runs the
    ///       render rather than by the synth, so they differ whenever the two runs were not told
    ///       to write to the same place. They are still compared and still reported: UTAU's
    ///       cache name carries six characters that stand for the arguments the note was
    ///       rendered with, and until we work out how those are made, cache reuse cannot be
    ///       turned on. Reporting it keeps that in sight.
    Comparison compare(const QList<hello::kit::SynthStep> &ours, const QList<ScriptCall> &theirs);

}

#endif // UTAUCOMPARE_SCRIPTCOMPARE_H
