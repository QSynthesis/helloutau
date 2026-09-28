#ifndef HELLOKIT_SYNTH_PITCHCURVE_H
#define HELLOKIT_SYNTH_PITCHCURVE_H

#include <optional>

#include <QtCore/QList>

#include <hellokit/Document/Note.h>

#include <hellokit/Synth/HelloKitSynthGlobal.h>

namespace hello::kit {

    /// The pitch curve of a note as UTAU passes it to the resampler, in cents relative to the
    /// note, in the two parts that an editor draws separately: the portamento of the Mode2
    /// points and the vibrato. Both include what the neighbouring notes contribute: the
    /// previous note before the start of this one, and the next note from its first point on.
    ///
    /// The rules are those of stdutau (\c synth.cpp, \c UtaPitchCurves), which the second stage
    /// compared with UTAU (docs/Synth.md), and a test compares the values with those of stdutau
    /// one by one. Among them:
    ///
    /// - The first point of a note after a note that is not a rest starts at the pitch of that
    ///   note, whatever its \c PBS gives, and a note without points bends from it at once.
    /// - Each point is converted to ticks with the tempo of this note, also those of the
    ///   neighbours.
    /// - The portamento is truncated to whole cents, the sum rounded.
    /// - A vibrato of 50 milliseconds or less is omitted on its own note but drawn, without
    ///   fading, where it reaches into a neighbour.
    ///
    /// Mode1 values (Note::pitchBend) are not part of the curve, as in stdutau.
    class HELLOKIT_SYNTH_EXPORT PitchCurve {
    public:
        /// The curve of note \a index of \a notes, whose tempo is \a tempo.
        PitchCurve(const QList<Note> &notes, qsizetype index, double tempo);

        /// The portamento at \a tick, counted from the start of the note.
        double portamentoAt(double tick) const;

        /// The vibrato at \a tick, counted from the start of the note.
        double vibratoAt(double tick) const;

        /// The timing of the samples of the note and the next one, in milliseconds, as
        /// reconciled with their neighbours (SynthStep::preUtterance and the like).
        struct Timing {
            double preUtterance = 0;
            double startPoint = 0;
            double nextPreUtterance = 0;
            double nextOverlap = 0;
        };

        /// The values the resampler receives: every five ticks from the pre-utterance and the
        /// start point before the note up to four ticks past its sample, the sum of the
        /// portamento and the vibrato rounded.
        QList<int> values(const Timing &timing) const;

    private:
        // A point in milliseconds and tenths of a semitone, as stdutau keeps it
        struct Point {
            double x = 0;
            double y = 0;
            PortamentoPoint::Type type = PortamentoPoint::S;
        };

        // A note as the curve reads it, its first point corrected to the previous note
        struct Part {
            QList<Point> points;
            std::optional<Vibrato> vibrato;
            int length = 480;
        };

        struct Impact {
            double portamento = 0;
            double vibrato = 0;
            // The next note counted relative to this one
            double shift = 0;
        };

        enum Whose {
            Own,
            Neighbour,
        };

        Part m_previous;
        Part m_current;
        std::optional<Part> m_next;
        double m_tempo;

        double ticksOf(double milliseconds) const;
        Impact impactOf(const Part &part, double tick, Whose whose) const;
        Impact previousAt(double tick) const;
        Impact currentAt(double tick) const;
        Impact nextAt(double tick, double previousTick) const;
        double nextStart() const;
    };

}

#endif // HELLOKIT_SYNTH_PITCHCURVE_H
