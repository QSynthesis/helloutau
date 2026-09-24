#ifndef HELLOKIT_DOCUMENT_PROJECT_H
#define HELLOKIT_DOCUMENT_PROJECT_H

#include <filesystem>
#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QByteArray>
#include <QtCore/QByteArrayView>
#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QString>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Document/DocumentConstants.h>
#include <hellokit/Document/HelloKitDocumentGlobal.h>
#include <hellokit/Document/Note.h>

namespace hello::kit {

    /// One voice part. A UST contains exactly one.
    struct Track {
        /// Not representable in UST, and therefore dropped on export to \c .ust.
        QString name;

        /// The voice bank directory. A \c %VOICE% prefix denotes the shared voice directory.
        QString voiceDir;

        QList<Note> notes;
    };

    /// Project-wide settings.
    struct ProjectSettings {
        QString name;
        double tempo = utau::DEFAULT_VALUE_TEMPO;
        QString flags;
        QString outputFile;

        /// The directory for cached render fragments. UTAU updates this to match the file name
        /// on save, and HelloUtau does the same.
        QString cacheDir;

        /// The engines specified by the project file, \c Tool1 and \c Tool2 in UST.
        ///
        /// \warning Untrusted, like \c Note::patch. Stored verbatim, because per-project engine
        ///          configuration is common UTAU practice and discarding it would delete user
        ///          settings. What is forbidden is executing them without confirmation, not
        ///          storing them. See the security section of AGENTS.md.
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
    /// The file is UTF-8 JSON without a byte order mark, with line breaks. docs/UsthFormat.md
    /// defines the format, and this implementation follows that definition. No encoding
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

        bool save(const std::filesystem::path &path, DiagnosticList &diagnostics) const;

        /// \overload
        static std::optional<Project> fromJson(QByteArrayView json, DiagnosticList &diagnostics);

        /// \overload
        QByteArray toJson() const;
    };

}

#endif // HELLOKIT_DOCUMENT_PROJECT_H
