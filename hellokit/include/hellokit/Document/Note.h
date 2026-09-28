#ifndef HELLOKIT_DOCUMENT_NOTE_H
#define HELLOKIT_DOCUMENT_NOTE_H

#include <array>
#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QDataStream>
#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringView>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Document/DocumentConstants.h>
#include <hellokit/Document/HelloKitDocumentGlobal.h>

namespace hello::kit {

    /// One envelope point: \c x in milliseconds from the previous point, \c y in percent.
    struct EnvelopeAnchor {
        double x = 0;
        double y = 0;

        inline bool operator==(const EnvelopeAnchor &RHS) const {
            return x == RHS.x && y == RHS.y;
        }

        inline bool operator!=(const EnvelopeAnchor &RHS) const {
            return !(*this == RHS);
        }
    };

    /// The volume envelope, with four anchors or with five including the middle anchor.
    ///
    /// Each index has a fixed role: 0 is the start, 1 the end of the attack, 2 the middle anchor,
    /// 3 the start of the release, and 4 the end. The index of an anchor therefore does not
    /// depend on whether the middle anchor exists. UST and \c .usth list the anchors in time
    /// order instead.
    ///
    /// \sa anchorsInTimeOrder()
    struct HELLOKIT_DOCUMENT_EXPORT Envelope {
        /// The anchors by role. Index 2 is part of the envelope only if \c hasMiddle is true.
        std::array<EnvelopeAnchor, 5> anchors;

        bool hasMiddle = false;

        /// Returns the four or five anchors of the envelope in time order.
        inline QList<EnvelopeAnchor> anchorsInTimeOrder() const {
            QList<EnvelopeAnchor> result{anchors[0], anchors[1]};
            if (hasMiddle) {
                result.append(anchors[2]);
            }
            result.append(anchors[3]);
            result.append(anchors[4]);
            return result;
        }

        /// Returns the envelope with \a anchors in time order, or \c std::nullopt unless it has
        /// four or five of them.
        static inline std::optional<Envelope> fromTimeOrder(const QList<EnvelopeAnchor> &anchors) {
            if (anchors.size() != 4 && anchors.size() != 5) {
                return std::nullopt;
            }
            Envelope envelope;
            envelope.hasMiddle = anchors.size() == 5;
            const int shift = envelope.hasMiddle ? 1 : 0;
            envelope.anchors[0] = anchors[0];
            envelope.anchors[1] = anchors[1];
            if (envelope.hasMiddle) {
                envelope.anchors[2] = anchors[2];
            }
            envelope.anchors[3] = anchors[2 + shift];
            envelope.anchors[4] = anchors[3 + shift];
            return envelope;
        }

        /// Returns whether both envelopes have the same anchors. An unused middle anchor is not
        /// compared.
        inline bool operator==(const Envelope &RHS) const {
            return hasMiddle == RHS.hasMiddle && anchors[0] == RHS.anchors[0] &&
                   anchors[1] == RHS.anchors[1] && (!hasMiddle || anchors[2] == RHS.anchors[2]) &&
                   anchors[3] == RHS.anchors[3] && anchors[4] == RHS.anchors[4];
        }

        inline bool operator!=(const Envelope &RHS) const {
            return !(*this == RHS);
        }

        /// Returns the envelope as written in \c .usth, with the anchors in time order.
        QJsonObject toJson() const;

        /// Returns the envelope written as in \c .usth, or \c std::nullopt unless \a object lists
        /// four or five anchors.
        static std::optional<Envelope> fromJson(const QJsonObject &object);
    };

    struct HELLOKIT_DOCUMENT_EXPORT Vibrato {
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

        inline bool operator==(const Vibrato &RHS) const {
            return length == RHS.length && period == RHS.period && amplitude == RHS.amplitude &&
                   attack == RHS.attack && release == RHS.release && phase == RHS.phase &&
                   offset == RHS.offset && intensity == RHS.intensity;
        }

        inline bool operator!=(const Vibrato &RHS) const {
            return !(*this == RHS);
        }

        QJsonObject toJson() const;

        /// Returns the vibrato written as in \c .usth. An absent parameter is read as zero.
        static Vibrato fromJson(const QJsonObject &object);
    };

    // The stream operators of the values stored as a whole in the edit history. They are
    // declared with the types, because Qt records the stream operators of a type where its
    // meta-type is first instantiated. The format is part of the history format and must not
    // change.

    inline QDataStream &operator<<(QDataStream &out, const Envelope &envelope) {
        for (const auto &anchor : envelope.anchors) {
            out << anchor.x << anchor.y;
        }
        return out << envelope.hasMiddle;
    }

    inline QDataStream &operator>>(QDataStream &in, Envelope &envelope) {
        for (auto &anchor : envelope.anchors) {
            in >> anchor.x >> anchor.y;
        }
        return in >> envelope.hasMiddle;
    }

    inline QDataStream &operator<<(QDataStream &out, const Vibrato &vibrato) {
        return out << vibrato.length << vibrato.period << vibrato.amplitude << vibrato.attack
                   << vibrato.release << vibrato.phase << vibrato.offset << vibrato.intensity;
    }

    inline QDataStream &operator>>(QDataStream &in, Vibrato &vibrato) {
        return in >> vibrato.length >> vibrato.period >> vibrato.amplitude >> vibrato.attack >>
               vibrato.release >> vibrato.phase >> vibrato.offset >> vibrato.intensity;
    }

    /// One control point of the Mode2 pitch curve.
    struct HELLOKIT_DOCUMENT_EXPORT PortamentoPoint {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::PortamentoPoint)
    public:
        /// The curve shape connecting a point to the preceding point.
        ///
        /// \warning The letters in the \c PBM entry of UST do not match these names. An empty
        ///          letter denotes \c S, \c s denotes \c Linear, \c r denotes \c R and \c j
        ///          denotes \c J. The mapping is stated here deliberately, so that only the UST
        ///          reader and writer depend on it.
        enum Type {
            S,
            Linear,
            R,
            J,
        };

        /// In milliseconds from the start of the note, for every point. Negative if the curve
        /// extends into the preceding note.
        ///
        /// \note UST writes the first point in \c PBS relative to the start of the note, and each
        ///       subsequent point in \c PBW as the interval from the preceding point.
        double x = 0;

        /// In cents.
        ///
        /// \note UST writes the height in tenths of a semitone, see centsFromTenths().
        double y = 0;

        Type type = S;

        /// Converts a height in tenths of a semitone, as \c PBS and \c PBY write it, to cents.
        /// The result is rounded to a millionth of a cent, so that a decimal read from a file
        /// gives the same decimal in cents rather than a value that differs in the last binary
        /// digit.
        static double centsFromTenths(double tenths);

        /// Converts a height in cents to tenths of a semitone, as \c PBS and \c PBY write it.
        static double tenthsFromCents(double cents);

        /// Returns the name of \a type in \c .usth, which is the name of the enumerator.
        static QString typeName(Type type);

        /// Returns the type named \a name in \c .usth, or \c std::nullopt if no type has this
        /// name.
        static std::optional<Type> typeFromName(QStringView name);

        QJsonObject toJson() const;

        /// Returns the point written as in \c .usth. An unknown curve type is reported in
        /// \a diagnostics and read as \c S.
        static PortamentoPoint fromJson(const QJsonObject &object, DiagnosticList &diagnostics);
    };

    /// The Mode1 pitch curve, with one value every five ticks.
    ///
    /// \note An empty value in the file is read as zero, as stdutau does, which is the most a
    ///       round trip through \c .ust can guarantee. No separate empty state exists, because
    ///       it could not be preserved.
    struct HELLOKIT_DOCUMENT_EXPORT PitchBend {
        std::optional<double> start;
        QList<double> values;

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

    /// One note of a track, or a rest.
    ///
    /// Notes are stored in sequence without absolute positions. The start of a note is the sum
    /// of the preceding lengths, including rests, which matches the layout of UST. In UST a rest
    /// is a regular note that may carry its own entries, so absolute positions would require
    /// synthesizing rests on export and discarding the entries of existing rests.
    ///
    /// An empty \c std::optional field was not specified in the file. This differs from a field
    /// specified as zero, and the distinction must survive a round trip.
    struct HELLOKIT_DOCUMENT_EXPORT Note {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::Note)
    public:
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
            return isRestLyric(lyric);
        }

        /// Returns whether a note with \a lyric is a rest: an empty lyric, \c R or \c r.
        static inline bool isRestLyric(QStringView lyric) {
            return lyric.isEmpty() ||
                   lyric.compare(QLatin1String(restLyric), Qt::CaseInsensitive) == 0;
        }

        /// Returns the note as written in \c .usth. Empty fields are omitted.
        QJsonObject toJson() const;

        /// Returns the note written as in \c .usth, or \c std::nullopt if the lyric, the length
        /// or the pitch is missing, with the reason in \a diagnostics. A malformed optional field
        /// is reported in \a diagnostics and read as absent.
        static std::optional<Note> fromJson(const QJsonObject &object, DiagnosticList &diagnostics);
    };

}

#endif // HELLOKIT_DOCUMENT_NOTE_H
