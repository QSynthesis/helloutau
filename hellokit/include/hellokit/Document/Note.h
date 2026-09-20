#ifndef HELLOKIT_DOCUMENT_NOTE_H
#define HELLOKIT_DOCUMENT_NOTE_H

#include <optional>

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>

#include <hellokit/Document/DocumentConstants.h>
#include <hellokit/Document/HelloKitDocumentGlobal.h>

namespace hello::kit {

    /// One point of the envelope, in milliseconds from the previous point and in percent.
    struct EnvelopeAnchor {
        double x = 0;
        double y = 0;
    };

    /// The volume envelope, four anchors or five.
    ///
    /// A fifth anchor is the optional one in the middle, and it sits at index 2 when it is there.
    struct Envelope {
        QList<EnvelopeAnchor> anchors;
    };

    struct Vibrato {
        double length = 0;    ///< percent of the note
        double period = 0;    ///< milliseconds
        double amplitude = 0; ///< cents
        double attack = 0;    ///< percent
        double release = 0;   ///< percent
        double phase = 0;     ///< percent
        double offset = 0;    ///< percent
    };

    /// How a portamento point joins the one before it.
    ///
    /// \warning The letters UST writes in \c PBM do not match these names. An empty letter is
    ///          \c S, the letter \c s is \c Linear, \c r is \c R and \c j is \c J. Written out
    ///          here on purpose, so that nothing but the UST reader and writer has to know that.
    enum class PortamentoType {
        S,
        Linear,
        R,
        J,
    };

    /// One control point of the Mode2 pitch curve.
    struct PortamentoPoint {
        /// Milliseconds. The first point is measured from the start of the note and may be
        /// negative, which reaches back into the note before it. Every other point is measured
        /// from the point before it.
        double x = 0;

        /// Tenths of a semitone.
        double y = 0;

        PortamentoType type = PortamentoType::S;
    };

    /// The Mode1 pitch curve, one reading every five ticks.
    struct PitchBend {
        std::optional<double> start;
        QList<int> values;

        /// What a reading holds where the curve has nothing there.
        ///
        /// A singular value rather than \c std::optional, which is the one place this project
        /// allows it. The curve is dense and long, and the same choice is already made in
        /// stdutau, so a second representation would only mean converting between them.
        static constexpr int noValue = -32768;
    };

    /// One note of a track, or a rest.
    ///
    /// Notes are stored in order and carry no absolute position. Where a note begins is the sum
    /// of the lengths before it, rests included, which is how UST itself is laid out. A rest is
    /// a real note there and can carry entries of its own, so giving notes absolute positions
    /// would mean inventing rests on the way out and dropping whatever those rests carried.
    ///
    /// Everything in \c std::optional is a field the file did not state. That is not the same as
    /// a field stated to be zero, and the difference has to survive a round trip.
    struct Note {
        QString lyric;   ///< \c R, \c r and an empty string are rests
        int length = 0;  ///< ticks, \c ticksPerQuarter to the quarter note
        int noteNum = 0; ///< 24 is C1, as in MIDI

        std::optional<double> intensity;
        std::optional<double> modulation;
        std::optional<double> velocity;
        std::optional<double> preUtterance;
        std::optional<double> voiceOverlap;
        std::optional<double> startPoint;

        /// The tempo from this note on. Empty means the note before it decides.
        std::optional<double> tempo;

        QString flags;

        std::optional<Envelope> envelope;
        std::optional<Vibrato> vibrato;
        QList<PortamentoPoint> portamento; ///< Mode2
        std::optional<PitchBend> pitchBend; ///< Mode1

        QString label;
        QString direct;

        /// A resampler named by the project file itself.
        ///
        /// \warning Untrusted. Stored as it was found, since configuring an engine per project is
        ///          ordinary UTAU practice and dropping it would delete the user's settings, but
        ///          never run without the user saying so. See the security section of AGENTS.md.
        QString patch;

        QString region;
        QString regionEnd;

        /// Every entry of the UST note that this structure has no field for, by the name UST
        /// gave it, \c $ prefix included.
        ///
        /// This is what makes \c .ust to \c .usth and back lossless. Values are UTF-8 here,
        /// converted from whatever encoding that file was in.
        QMap<QString, QString> userData;

        /// Whether this note makes no sound, which is what UTAU decides from the lyric alone.
        bool isRest() const {
            return lyric.isEmpty() || lyric.compare(QLatin1String(restLyric), Qt::CaseInsensitive) == 0;
        }
    };

}

#endif // HELLOKIT_DOCUMENT_NOTE_H
