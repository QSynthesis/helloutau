#ifndef HELLOKIT_VOICEBANK_VOICEBANKCONFIG_H
#define HELLOKIT_VOICEBANK_VOICEBANKCONFIG_H

#include <filesystem>
#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QByteArray>
#include <QtCore/QByteArrayView>
#include <QtCore/QJsonObject>
#include <QtCore/QString>

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

        /// The encoding of the UTAU files in this directory: \c oto.ini , \c prefix.map ,
        /// \c character.txt and \c readme.txt .
        ///
        /// Never detected automatically. The value is the encoding specified by the user,
        /// recorded for later sessions.
        QString charset;

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
