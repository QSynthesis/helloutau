#ifndef HELLOKIT_VOICEBANK_VOICEBANKCONFIG_H
#define HELLOKIT_VOICEBANK_VOICEBANKCONFIG_H

#include <filesystem>
#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QByteArray>
#include <QtCore/QByteArrayView>
#include <QtCore/QJsonObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>

namespace hello::kit {

    /// The HelloUtau configuration of one voice bank directory.
    ///
    /// **One per directory, not one per voice bank.** A voice bank may contain several
    /// \c oto.ini files in different subdirectories, possibly in different encodings. The
    /// configuration is therefore stored beside the files it describes, and is carried along
    /// when a subdirectory is copied separately.
    ///
    /// \sa docs/note.md
    struct HELLOKIT_VOICEBANK_EXPORT VoiceBankConfig {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::VoiceBankConfig)
    public:
        /// The name of the file in a voice bank directory.
        static constexpr char fileName[] = "hello-config.json";

        /// The encoding of the UTAU files in this directory: the \c oto.ini , and in the root also
        /// \c prefix.map , \c character.txt and \c readme.txt . An \c oto.ini that declares
        /// UTF-8 takes precedence over it.
        ///
        /// Never detected automatically. The value is the encoding specified by the user,
        /// recorded for later sessions.
        ///
        /// \sa VoiceBankDirectory::charset, VoiceBankDirectorySource::fileNamed()
        QString charset;

        /// The encodings of the misdecoded file names in one directory, and the state of each
        /// name. See the section on misdecoded file names in docs/Editing.md.
        ///
        /// A name is misdecoded if its bytes in \a original were interpreted in \a misdecodedAs
        /// when the file was written to the file system, for example by an archive extractor.
        /// The record governs the audio files and the subdirectories of its own directory only.
        /// Every name of them that is not ASCII belongs to exactly one of the two lists, and a
        /// name in neither list is a new name whose state the user determines.
        struct HELLOKIT_VOICEBANK_EXPORT FileNameEncoding {
            /// The encoding in which the names were created.
            QString original;
            /// The encoding in which the bytes of the names were interpreted.
            QString misdecodedAs;
            /// The names on disk that are misdecoded, which are restored when read.
            QStringList misdecoded;
            /// The names on disk that are correct, which are used as they are.
            QStringList correct;

            /// Returns the name that \a physical , a name on disk, had before it was misdecoded, in
            /// Normalization Form C. A correct name often restores as well, therefore the result
            /// is not evidence that \a physical is misdecoded.
            ///
            /// \return \c std::nullopt if \a physical is ASCII, an encoding is unavailable,
            ///         \a physical is unrepresentable in \a misdecodedAs , the bytes are invalid in
            ///         \a original , or the result equals \a physical or contains a path separator
            ///         or a control character
            std::optional<QString> restore(const QString &physical) const;

            /// Returns the name under which a voice bank uses \a physical , a name on disk: the
            /// restored name if \a misdecoded lists \a physical in any normalization form and it
            /// restores, otherwise \a physical .
            QString logicalNameOf(const QString &physical) const;

            inline bool operator==(const FileNameEncoding &RHS) const {
                return original == RHS.original && misdecodedAs == RHS.misdecodedAs &&
                       misdecoded == RHS.misdecoded && correct == RHS.correct;
            }
            inline bool operator!=(const FileNameEncoding &RHS) const {
                return !(*this == RHS);
            }
        };

        /// The record of misdecoded file names, or \c std::nullopt if the names of the directory
        /// are used as they are.
        ///
        /// A malformed record is read as \c std::nullopt with a warning, and is kept in
        /// \a unknownFields so that saving writes it back unchanged.
        std::optional<FileNameEncoding> fileNameEncoding;

        /// Top-level fields not recognized by this version, preserved so that saving writes them
        /// back.
        QJsonObject unknownFields;

        /// \return the configuration, or \c std::nullopt if the file is missing or unreadable,
        ///         with the reason in \a diagnostics
        static std::optional<VoiceBankConfig> open(const std::filesystem::path &path,
                                                   DiagnosticList &diagnostics);

        bool save(const std::filesystem::path &path, DiagnosticList &diagnostics) const;

        /// \overload
        static std::optional<VoiceBankConfig> fromJson(QByteArrayView json,
                                                       DiagnosticList &diagnostics);

        /// \overload
        QByteArray toJson() const;
    };

}

#endif // HELLOKIT_VOICEBANK_VOICEBANKCONFIG_H
