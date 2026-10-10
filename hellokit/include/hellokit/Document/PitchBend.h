#ifndef HELLOKIT_DOCUMENT_PITCHBEND_H
#define HELLOKIT_DOCUMENT_PITCHBEND_H

#include <optional>

#include <QtCore/QJsonObject>
#include <QtCore/QList>

#include <hellokit/Document/HelloKitDocumentGlobal.h>

namespace hello::kit {

    struct Note;
    struct PreviousBend;

    /// The Mode1 pitch curve, with one value every five ticks.
    ///
    /// \note An empty value in the file is read as zero, as stdutau does, which is the most a
    ///       round trip through \c .ust can guarantee. No separate empty state exists, because
    ///       it could not be preserved.
    struct HELLOKIT_DOCUMENT_EXPORT PitchBend {
        std::optional<double> start; ///< \c PBStart, in milliseconds from the start of the note
        QList<double> values;        ///< \c PitchBend, in cents

        /// The curve of Mode1 of a note with \a bend at \a tick, counted from the start of the
        /// note, as UTAU passes it to the resampler (docs/Synth.md, "Mode1 的音高"): value k lies
        /// 5 k ticks after the start, and between two values the curve is their linear
        /// interpolation. The interval after the last value holds it. Elsewhere the curve is 0,
        /// but before the start of the note, where the values of \a previous reach with the
        /// interval after them, it is those values moved by the offset of \a previous. This
        /// holds before the first value and also after the values end before the start of the
        /// note. The starts of both convert to ticks at \a tempo, the tempo of the note.
        static double curveAt(const std::optional<PitchBend> &bend, const PreviousBend &previous,
                              double tick, double tempo);

        /// \a bend with \a values drawn over it, the first at \a tick from the start of the note
        /// and the others every five ticks after it, at \a tempo; \a previous is that of
        /// curveAt().
        ///
        /// Without values, \a bend starts at \a tick, to a thousandth of a millisecond as UTAU
        /// writes \c PBStart. Otherwise it keeps its start, \a tick is rounded to the nearest of
        /// its positions, and its values extend before or after as far as \a values do. The
        /// values between those drawn and those it had take the curve as it was, rounded, so
        /// that what is not drawn sounds as before.
        static PitchBend drawn(const std::optional<PitchBend> &bend, const PreviousBend &previous,
                               double tempo, double tick, const QList<double> &values);

        inline bool operator==(const PitchBend &RHS) const {
            return start == RHS.start && values == RHS.values;
        }

        inline bool operator!=(const PitchBend &RHS) const {
            return !(*this == RHS);
        }

        QJsonObject toJson() const;

        /// Returns the pitch curve written as in \c .usth. A start that is not a number is read
        /// as absent.
        static PitchBend fromJson(const QJsonObject &object);
    };

    /// What the Mode1 curve of a note takes from the note before it (PitchBend::curveAt()).
    struct HELLOKIT_DOCUMENT_EXPORT PreviousBend {
        std::optional<PitchBend> bend; ///< its Mode1 values
        int length = 0;                ///< its length in ticks
        /// Its pitch relative to the note, in cents, or 0 after a rest, as for the first Mode2
        /// point (PitchCurve)
        double offset = 0;

        /// That of \a previous for \a note.
        static PreviousBend of(const Note &previous, const Note &note);
    };

}

#endif // HELLOKIT_DOCUMENT_PITCHBEND_H
