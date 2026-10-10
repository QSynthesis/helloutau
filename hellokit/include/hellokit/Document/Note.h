#ifndef HELLOKIT_DOCUMENT_NOTE_H
#define HELLOKIT_DOCUMENT_NOTE_H

#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QStringView>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Document/DocumentConstants.h>
#include <hellokit/Document/Envelope.h>
#include <hellokit/Document/HelloKitDocumentGlobal.h>
#include <hellokit/Document/PitchBend.h>
#include <hellokit/Document/PortamentoPoint.h>
#include <hellokit/Document/Vibrato.h>

namespace hello::kit {

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

        /// \c $direct of UTAU. With any value but an empty one, the wavtool appends the sample
        /// of the voice bank itself, without the resampler. See docs/Synth.md.
        QString direct;

        /// \c $patch of UTAU, the name of a wav file relative to the folder of the project file,
        /// which the wavtool appends in place of the sample, without the resampler. The note is
        /// silent if the file is missing. See docs/Synth.md.
        ///
        /// \warning Untrusted, like every path of a project file. The file is only read.
        QString patch;

        /// The names of the regions that start at this note, and of those that end at it, in the
        /// order of the UST, which writes each as \c $region and \c $region_end joined with
        /// \c |. A name is never empty and never contains \c |.
        QStringList regions;
        QStringList regionEnds;

        /// Returns the region names of \a value, a \c $region or \c $region_end of the UST:
        /// the parts between \c | that are not empty. An illegal value, such as one with an
        /// empty part, therefore does not survive a conversion unchanged.
        static QStringList regionNamesFromUst(const QString &value);

        /// Returns \a names joined with \c | for \c $region or \c $region_end of the UST.
        static QString regionNamesToUst(const QStringList &names);

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

        /// Returns whether a note with \a lyric is a rest: an empty or whitespace-only lyric, or
        /// \c R or \c r.
        static inline bool isRestLyric(QStringView lyric) {
            const auto trimmed = lyric.trimmed();
            return trimmed.isEmpty() ||
                   trimmed.compare(QLatin1String(restLyric), Qt::CaseInsensitive) == 0;
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
