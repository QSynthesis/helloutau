#ifndef HELLOKIT_DOCUMENT_NOTE_H
#define HELLOKIT_DOCUMENT_NOTE_H

#include <optional>

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>

#include <hellokit/Document/DocumentConstants.h>
#include <hellokit/Document/HelloKitDocumentGlobal.h>

namespace hello::kit {

    /// One envelope point: \c x in milliseconds from the previous point, \c y in percent.
    struct EnvelopeAnchor {
        double x = 0;
        double y = 0;
    };

    /// The volume envelope, with four or five anchors.
    ///
    /// The optional fifth anchor lies in the middle, at index 2 when present.
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

        /// The eighth value of \c VBR in UST, which UTAU ignores.
        ///
        /// Retained so that a round trip through \c .ust preserves it. The UTAU interface
        /// exposes no field for it, and it cannot be stored in \c Note::userData , because each
        /// value there is written as a separate entry, whereas this one is part of \c VBR.
        double intensity = 0;
    };

    /// The curve shape connecting a portamento point to the preceding point.
    ///
    /// \warning The letters in the \c PBM entry of UST do not match these names. An empty
    ///          letter denotes \c S, \c s denotes \c Linear, \c r denotes \c R and \c j denotes
    ///          \c J. The mapping is stated here deliberately, so that only the UST reader and
    ///          writer depend on it.
    enum class PortamentoType {
        S,
        Linear,
        R,
        J,
    };

    /// One control point of the Mode2 pitch curve.
    struct PortamentoPoint {
        /// In milliseconds. The first point is relative to the start of the note and may be
        /// negative, extending into the preceding note. Each subsequent point is relative to the
        /// preceding point.
        double x = 0;

        /// In tenths of a semitone.
        double y = 0;

        PortamentoType type = PortamentoType::S;
    };

    /// The Mode1 pitch curve, with one value every five ticks.
    ///
    /// \note An empty value in the file is read as zero, as stdutau does, which is the most a
    ///       round trip through \c .ust can guarantee. No separate empty state exists, because
    ///       it could not be preserved.
    struct PitchBend {
        std::optional<double> start;
        QList<double> values;
    };

    /// One note of a track, or a rest.
    ///
    /// Notes are stored in sequence without absolute positions. The start of a note is the sum
    /// of the preceding lengths, including rests, which matches the layout of UST. In UST a rest
    /// is a regular note that may carry its own entries, so absolute positions would require
    /// synthesizing rests on export and discarding the entries of existing rests.
    ///
    /// An empty \c std::optional field was not specified in the file. This differs from a field
    /// specified as zero, and the distinction must survive a round trip.
    struct Note {
        QString lyric;   ///< \c R, \c r and an empty string denote rests
        int length = 0;  ///< in ticks, \c ticksPerQuarter per quarter note
        int noteNum = 0; ///< 24 is C1, as in MIDI

        std::optional<double> intensity;
        std::optional<double> modulation;
        std::optional<double> velocity;
        std::optional<double> preUtterance;
        std::optional<double> voiceOverlap;
        std::optional<double> startPoint;

        /// The tempo from this note onward. Empty if inherited from the preceding note.
        std::optional<double> tempo;

        QString flags;

        std::optional<Envelope> envelope;
        std::optional<Vibrato> vibrato;
        QList<PortamentoPoint> portamento;  ///< Mode2
        std::optional<PitchBend> pitchBend; ///< Mode1

        QString label;
        QString direct;

        /// A resampler specified by the project file.
        ///
        /// \warning Untrusted. Stored verbatim, because per-project engine configuration is
        ///          common UTAU practice and discarding it would delete user settings, but never
        ///          executed without explicit user consent. See the security section of
        ///          AGENTS.md.
        QString patch;

        QString region;
        QString regionEnd;

        /// Every entry of the UST note without a corresponding field, keyed by its UST name
        /// including any \c $ prefix.
        ///
        /// This makes the conversion from \c .ust to \c .usth and back lossless. Values are
        /// stored as UTF-8, converted from the encoding of the source file.
        QMap<QString, QString> userData;

        /// Returns whether the note is a rest. UTAU determines this from the lyric alone.
        inline bool isRest() const {
            return lyric.isEmpty() ||
                   lyric.compare(QLatin1String(restLyric), Qt::CaseInsensitive) == 0;
        }
    };

}

#endif // HELLOKIT_DOCUMENT_NOTE_H
