#ifndef HELLOKIT_DOCUMENT_PROJECT_H
#define HELLOKIT_DOCUMENT_PROJECT_H

#include <filesystem>
#include <optional>
#include <vector>

#include <QtCore/QCoreApplication>
#include <QtCore/QByteArray>
#include <QtCore/QByteArrayView>
#include <QtCore/QDataStream>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QString>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Document/DocumentConstants.h>
#include <hellokit/Document/HelloKitDocumentGlobal.h>
#include <hellokit/Document/Note.h>

namespace hello::kit {

    /// The directories against which the \c VoiceDir of a track is resolved.
    struct HELLOKIT_DOCUMENT_EXPORT VoiceLocations {
        /// The voice folders that the \c %VOICE% prefix denotes, in decreasing priority. A
        /// folder that does not exist is skipped.
        std::vector<std::filesystem::path> voiceFolders;

        /// The directory against which a relative \c VoiceDir without the prefix is resolved,
        /// or an empty path if none.
        std::filesystem::path relativeBase;

        /// Returns the locations as UTAU uses them: \c %VOICE% denotes the \c voice directory
        /// in \a utauDirectory, and a relative path is relative to \a utauDirectory. Both are
        /// empty if \a utauDirectory is empty.
        static VoiceLocations ofUtau(const std::filesystem::path &utauDirectory);

        bool operator==(const VoiceLocations &other) const;
        bool operator!=(const VoiceLocations &other) const;
    };

    /// A named stretch of the notes of a track, as UTAU names one: from the note whose
    /// Note::regions holds the name to the first note from there whose Note::regionEnds holds
    /// it, or to the last note of the track if no note does.
    struct HELLOKIT_DOCUMENT_EXPORT Region {
        QString name;
        int first = 0;
        int last = 0;

        inline bool operator==(const Region &other) const {
            return name == other.name && first == other.first && last == other.last;
        }

        inline bool operator!=(const Region &other) const {
            return !(*this == other);
        }

        /// Returns the regions of the notes whose Note::regions are \a starts and whose
        /// Note::regionEnds are \a ends, both indexed by note, in the order of their first notes
        /// and, for the regions of one note, in the order of its names.
        static QList<Region> of(const QList<QStringList> &starts, const QList<QStringList> &ends);
    };

    /// One voice part. A UST contains exactly one.
    struct HELLOKIT_DOCUMENT_EXPORT Track {
        /// Not representable in UST, and therefore dropped on export to \c .ust.
        QString name;

        /// The voice bank directory as the file writes it. See voiceDirectory() for its meaning.
        QString voiceDir;

        QList<Note> notes;

        /// The prefix of \c voiceDir that denotes a voice folder.
        ///
        /// TODO: Move UTAU-specific path expansion and serialization to a compatibility class.
        static constexpr QStringView voicePrefix = u"%VOICE%";

        /// Returns the voice bank directory that \c voiceDir denotes.
        ///
        /// A \c %VOICE% prefix denotes the first voice folder of \a locations that contains the
        /// remainder of the path. A relative path is relative to the \c relativeBase of
        /// \a locations, not to the project file. UTAU resolves both against the directory of
        /// \c utau.exe, see docs/claude/utau-voicedir-cachedir.md.
        ///
        /// \return the directory, which exists unless no voice folder contains it. If no voice
        ///         folder contains a \c %VOICE% path, the path in the first existing voice folder
        ///         is returned. An empty path is returned if \c voiceDir is empty, if no voice
        ///         folder exists for a \c %VOICE% path, or if \c relativeBase is empty for a
        ///         relative path.
        std::filesystem::path voiceDirectory(const VoiceLocations &locations) const;

        /// Returns the value of \c voiceDir for the voice bank in \a directory, as UTAU writes it
        /// on save: with the \c %VOICE% prefix if \a directory is inside a voice folder of
        /// \a locations, and otherwise the absolute path.
        ///
        /// The prefix is used only if it resolves to \a directory again. A folder of the same
        /// name in a voice folder of higher priority would otherwise take its place, and the
        /// absolute path is written instead.
        ///
        /// \param directory an absolute path
        static QString voiceDirOf(const std::filesystem::path &directory,
                                  const VoiceLocations &locations);

        /// Returns the regions of the notes, see Region::of().
        QList<Region> regions() const;
    };

    /// The time signature of a project, which only the editor uses, for the bars and beats of
    /// the ruler and the grid. UST has no time signature, and the control note carries it, see
    /// docs/UsthFormat.md.
    struct HELLOKIT_DOCUMENT_EXPORT TimeSignature {
        /// The largest number of beats per bar.
        static constexpr int maximumNumerator = 32;

        /// The beat units, as the denominator writes them.
        static constexpr int denominators[] = {2, 4, 8, 16, 32};

        int numerator = 4;
        int denominator = 4;

        /// Returns whether \a numerator is from 1 to maximumNumerator and \a denominator is one
        /// of denominators.
        static bool isValid(int numerator, int denominator);

        inline bool operator==(const TimeSignature &RHS) const {
            return numerator == RHS.numerator && denominator == RHS.denominator;
        }

        inline bool operator!=(const TimeSignature &RHS) const {
            return !(*this == RHS);
        }

        /// Returns the time signature as written in \c .usth and in the control note.
        QJsonObject toJson() const;

        /// Returns the time signature written as in \c .usth, or \c std::nullopt unless
        /// \a object holds a valid numerator and denominator.
        static std::optional<TimeSignature> fromJson(const QJsonObject &object);
    };

    // The stream operators of a value stored as a whole in the edit history, see the operators
    // of Envelope in Envelope.h. The format is part of the history format and must not change.

    inline QDataStream &operator<<(QDataStream &out, const TimeSignature &timeSignature) {
        return out << qint32(timeSignature.numerator) << qint32(timeSignature.denominator);
    }

    inline QDataStream &operator>>(QDataStream &in, TimeSignature &timeSignature) {
        qint32 numerator = 0;
        qint32 denominator = 0;
        in >> numerator >> denominator;
        timeSignature = {numerator, denominator};
        return in;
    }

    /// Project-wide settings.
    struct ProjectSettings {
        QString name;
        double tempo = utau::DEFAULT_VALUE_TEMPO;
        QString flags;
        QString outputFile;

        /// The directory for cached render fragments, as the file writes it.
        ///
        /// UTAU does not use this value: the cache is always the directory named by
        /// Project::cacheDirTextOf() beside the project file, and saving writes that name here.
        /// HelloUtau does the same, see Project::cacheDirectoryOf().
        QString cacheDir;

        /// The synth tools specified by the project file, \c Tool1 and \c Tool2 in UST.
        ///
        /// \warning Untrusted, like \c Note::patch. Stored verbatim, because per-project synth tool
        ///          configuration is common UTAU practice and discarding it would delete user
        ///          settings. What is forbidden is executing them without confirmation, not
        ///          storing them. See the security section of CLAUDE.md.
        QString wavtool;
        QString resampler;

        /// Whether pitch uses the Mode2 curve rather than the Mode1 value array. A project uses
        /// exactly one mode, so \c Note::portamento and \c Note::pitchBend are never both set.
        bool mode2 = true;

        /// 4/4 if the file records none.
        TimeSignature timeSignature;
    };

    /// An in-memory project, the common representation of \c .usth, \c .ust and every imported
    /// format.
    ///
    /// Reading and writing \c .usth belongs to this class rather than to a separate one,
    /// because \c .usth is not one format among several but the native serialization of a
    /// project. Every other format is converted through \c HelloKitInterchange into this class.
    ///
    /// The file is UTF-8 JSON without a byte order mark, written compact by default, without
    /// indentation or line breaks, because a project with many notes would otherwise grow
    /// several times in size. docs/UsthFormat.md defines the format, and this implementation
    /// follows that definition. No encoding
    /// detection is required, which distinguishes it from reading a \c .ust.
    ///
    /// \note \c tracks currently holds exactly one track. The array exists from the start so
    ///       that multiple tracks can be added without a new format version. A reader that
    ///       encounters any other length must report it rather than silently use the first
    ///       track.
    struct HELLOKIT_DOCUMENT_EXPORT Project {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::Project)
    public:
        ProjectSettings settings;
        QList<Track> tracks;

        /// Top-level fields of the \c .usth file not recognized by this version, preserved so
        /// that saving writes them back.
        ///
        /// Otherwise an older build opening a project saved by a newer one would discard
        /// unrecognized data, and the user would find it missing after saving. Empty for a
        /// project from any other source.
        QJsonObject unknownFields;

        /// \return the project, or \c std::nullopt if the file could not be parsed, with the
        ///         reason in \a diagnostics
        static std::optional<Project> open(const std::filesystem::path &path,
                                           DiagnosticList &diagnostics);

        /// Writes the project to \a path, with \c ProjectSettings::cacheDir replaced by
        /// cacheDirTextOf() \a path, as UTAU does on save. \a format \c Indented writes a file that
        /// is easier to read and to compare, several times larger.
        bool save(const std::filesystem::path &path, DiagnosticList &diagnostics,
                  QJsonDocument::JsonFormat format = QJsonDocument::Compact) const;

        /// Returns the value of \c ProjectSettings::cacheDir for a project saved as \a file, as
        /// UTAU writes it on save: the file name without its extension, followed by \c .cache.
        /// See docs/claude/utau-voicedir-cachedir.md.
        static QString cacheDirTextOf(const std::filesystem::path &file);

        /// Returns the directory of the render cache for a project saved as \a file: the
        /// directory named cacheDirTextOf() beside \a file. UTAU uses this directory whatever the
        /// file specifies.
        static std::filesystem::path cacheDirectoryOf(const std::filesystem::path &file);

        /// Returns the path \a text with the separators that a saved project uses, whatever the
        /// platform: slashes in an absolute Unix path, and backslashes, as UTAU writes them, in
        /// any other path. Nothing else changes, so that \c .. and \c %VOICE% keep their meaning.
        static QString savedPathText(const QString &text);

        /// Returns the path that the path text \a text of a project specifies, with backslashes
        /// read as separators on every platform, because UTAU writes them.
        static std::filesystem::path pathOf(const QString &text);

        /// \overload
        static std::optional<Project> fromJson(QByteArrayView json, DiagnosticList &diagnostics);

        /// \overload
        QByteArray toJson(QJsonDocument::JsonFormat format = QJsonDocument::Compact) const;
    };

}

#endif // HELLOKIT_DOCUMENT_PROJECT_H
