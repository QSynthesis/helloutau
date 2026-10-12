#ifndef HELLOKIT_VOICEBANK_VOICEBANKCONFIGURATION_H
#define HELLOKIT_VOICEBANK_VOICEBANKCONFIGURATION_H

#include <filesystem>
#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QByteArray>
#include <QtCore/QByteArrayView>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
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
    struct HELLOKIT_VOICEBANK_EXPORT VoiceBankConfiguration {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::VoiceBankConfiguration)
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

        /// The decision of the user about the file names in one directory. See the section on
        /// misdecoded file names in docs/Editing.md.
        ///
        /// A name is misdecoded if its bytes in \a original were interpreted in \a misdecodedAs
        /// when the file was written to the file system, for example by an archive extractor.
        /// The record governs the audio files and the subdirectories of its own directory only,
        /// in one of three forms:
        /// - Without encodings, every name is used as it is.
        /// - With encodings and without \a files , every name is restored.
        /// - With encodings and \a files , every name that is not ASCII belongs to exactly one of
        ///   the two lists, and a name in neither list is a new name whose state the user
        ///   determines.
        struct HELLOKIT_VOICEBANK_EXPORT FileNameEncoding {
            /// The names on disk of a directory whose names are partly misdecoded.
            struct Files {
                /// The names on disk that are misdecoded, which are restored when read.
                QStringList misdecoded;
                /// The names on disk that are correct, which are used as they are.
                QStringList correct;

                inline bool operator==(const Files &RHS) const {
                    return misdecoded == RHS.misdecoded && correct == RHS.correct;
                }
                inline bool operator!=(const Files &RHS) const {
                    return !(*this == RHS);
                }
            };

            /// The encoding in which the names were created, empty if the names are used as they
            /// are.
            QString original;
            /// The encoding in which the bytes of the names were interpreted, empty if the names
            /// are used as they are.
            QString misdecodedAs;
            /// The state of each name, or \c std::nullopt if every name has the same state.
            std::optional<Files> files;

            /// Returns whether the record has no encodings, so that every name is used as it is.
            inline bool isAsIs() const {
                return original.isEmpty() && misdecodedAs.isEmpty();
            }

            /// Returns the record of the names \a misdecoded and \a correct under the encodings
            /// \a original and \a misdecodedAs in the form that it is written in: without
            /// encodings if no name is misdecoded, and without lists if no correct name restores.
            static FileNameEncoding normalized(const QString &original, const QString &misdecodedAs,
                                               const QStringList &misdecoded,
                                               const QStringList &correct);

            /// Returns the record of \a json , the value of \c fileNameEncoding in
            /// hello-config.json, or \c std::nullopt if it is malformed: not an object, with an
            /// unknown field or a field of another type, with one encoding only, or with
            /// \c files but without encodings or without either list.
            static std::optional<FileNameEncoding> fromJson(const QJsonValue &json);

            /// Returns the value of \c fileNameEncoding in hello-config.json.
            QJsonObject toJson() const;

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
            /// restored name if \a physical is misdecoded and restores, otherwise \a physical . A
            /// name is misdecoded if the record has encodings and either has no \a files or lists
            /// the name as misdecoded in any normalization form.
            QString logicalNameOf(const QString &physical) const;

            inline bool operator==(const FileNameEncoding &RHS) const {
                return original == RHS.original && misdecodedAs == RHS.misdecodedAs &&
                       files == RHS.files;
            }
            inline bool operator!=(const FileNameEncoding &RHS) const {
                return !(*this == RHS);
            }
        };

        /// The record of the file names, or \c std::nullopt if the user has not decided. The names
        /// of a directory without a record are used as they are.
        ///
        /// A malformed record is read as \c std::nullopt with a warning, and is kept in
        /// \a unknownFields so that saving writes it back unchanged.
        std::optional<FileNameEncoding> fileNameEncoding;

        /// Top-level fields not recognized by this version, preserved so that saving writes them
        /// back.
        QJsonObject unknownFields;

        /// \return the configuration, or \c std::nullopt if the file is missing or unreadable,
        ///         with the reason in \a diagnostics
        static std::optional<VoiceBankConfiguration> open(const std::filesystem::path &path,
                                                          DiagnosticList &diagnostics);

        bool save(const std::filesystem::path &path, DiagnosticList &diagnostics) const;

        /// \overload
        static std::optional<VoiceBankConfiguration> fromJson(QByteArrayView json,
                                                              DiagnosticList &diagnostics);

        /// \overload
        QByteArray toJson() const;
    };

}

#endif // HELLOKIT_VOICEBANK_VOICEBANKCONFIGURATION_H
