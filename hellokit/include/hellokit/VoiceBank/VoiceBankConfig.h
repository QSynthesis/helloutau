#ifndef HELLOKIT_VOICEBANK_VOICEBANKCONFIG_H
#define HELLOKIT_VOICEBANK_VOICEBANKCONFIG_H

#include <filesystem>
#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QByteArrayView>
#include <QtCore/QJsonObject>
#include <QtCore/QString>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>

namespace hello::kit {

    /// What HelloUTAU's own file in a voice bank directory is called.
    inline constexpr char voiceBankConfigFileName[] = "hello-config.json";

    /// HelloUTAU's record for one directory of a voice bank.
    ///
    /// **One per directory, not one per bank.** A bank may spread several \c oto.ini over its
    /// subdirectories and they need not be in the same encoding, so the record sits beside the
    /// files it describes and goes with them when a subdirectory is copied out on its own.
    ///
    /// \sa docs/note.md
    struct HELLOKIT_VOICEBANK_EXPORT VoiceBankConfig {
        /// The encoding of the UTAU files in this directory: \c oto.ini , \c prefix.map ,
        /// \c character.txt and \c readme.txt .
        ///
        /// Nothing guesses it. It is here because a user was asked once and the answer was
        /// written down.
        QString charset;

        /// Top level fields this version has no field for, kept so that they are written back.
        QJsonObject unknownFields;

        /// \return the record, or nothing where the file is missing or unreadable, with the
        ///         reason in \a diagnostics
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
