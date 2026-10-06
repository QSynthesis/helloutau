#ifndef HELLOKIT_DOCUMENT_PROJECT_H
#define HELLOKIT_DOCUMENT_PROJECT_H

#include <filesystem>
#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QByteArray>
#include <QtCore/QByteArrayView>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QString>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Document/DocumentConstants.h>
#include <hellokit/Document/HelloKitDocumentGlobal.h>
#include <hellokit/Document/Note.h>

namespace hello::kit {

    /// One voice part. A UST contains exactly one.
    struct HELLOKIT_DOCUMENT_EXPORT Track {
        /// Not representable in UST, and therefore dropped on export to \c .ust.
        QString name;

        /// The voice bank directory as the file writes it. See voiceDirectory() for its meaning.
        QString voiceDir;

        QList<Note> notes;

        /// The prefix of \c voiceDir that denotes the \c voice directory of the UTAU
        /// installation.
        ///
        /// TODO: Move UTAU-specific path expansion and serialization to a compatibility class.
        static constexpr QStringView voicePrefix = u"%VOICE%";

        /// Returns the voice bank directory that \c voiceDir denotes, resolved as UTAU resolves
        /// it: a \c %VOICE% prefix denotes the \c voice directory in \a utauDirectory, and a
        /// relative path is relative to \a utauDirectory, not to the project file. See
        /// docs/claude/utau-voicedir-cachedir.md.
        ///
        /// \param utauDirectory the directory that contains \c utau.exe, or an empty path if
        ///                      unknown
        /// \return an empty path if \c voiceDir is empty, or if it requires \a utauDirectory and
        ///         that is empty
        std::filesystem::path voiceDirectory(const std::filesystem::path &utauDirectory) const;

        /// Returns the value of \c voiceDir for the voice bank in \a directory, as UTAU writes it
        /// on save: with the \c %VOICE% prefix if \a directory is inside the \c voice directory
        /// of \a utauDirectory, otherwise the absolute path.
        ///
        /// \param directory an absolute path
        /// \param utauDirectory the directory that contains \c utau.exe, or an empty path if
        ///                      unknown
        static QString voiceDirOf(const std::filesystem::path &directory,
                                  const std::filesystem::path &utauDirectory);
    };

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

        /// The engines specified by the project file, \c Tool1 and \c Tool2 in UST.
        ///
        /// \warning Untrusted, like \c Note::patch. Stored verbatim, because per-project engine
        ///          configuration is common UTAU practice and discarding it would delete user
        ///          settings. What is forbidden is executing them without confirmation, not
        ///          storing them. See the security section of CLAUDE.md.
        QString wavtool;
        QString resampler;

        /// Whether pitch uses the Mode2 curve rather than the Mode1 value array. A project uses
        /// exactly one mode, so \c Note::portamento and \c Note::pitchBend are never both set.
        bool mode2 = true;
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

        /// \overload
        static std::optional<Project> fromJson(QByteArrayView json, DiagnosticList &diagnostics);

        /// \overload
        QByteArray toJson(QJsonDocument::JsonFormat format = QJsonDocument::Compact) const;
    };

}

#endif // HELLOKIT_DOCUMENT_PROJECT_H
