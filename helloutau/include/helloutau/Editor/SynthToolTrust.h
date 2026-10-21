#ifndef HELLOUTAU_EDITOR_SYNTHTOOLTRUST_H
#define HELLOUTAU_EDITOR_SYNTHTOOLTRUST_H

#include <filesystem>
#include <optional>

#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Support/FileIntegrity.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QWidget;

namespace hello::daw {

    class AppSettings;

    /// Trust of the synth tools that a project specifies, the wavtool and the resampler.
    ///
    /// A project file is untrusted input, and a synth tool path that it specifies runs only after
    /// the user has trusted the synth tool. A trust record holds the canonical path of the synth
    /// tool and the SHA-256 of its file, so that a changed file is no longer trusted. The records
    /// are kept in the application settings, one for each path. A relative synth tool path is
    /// resolved against the UTAU directory of the settings. See the section on security in
    /// docs/note.md and the integrity of the trusted programs in docs/ProgramIntegrity.md.
    class HELLOUTAU_EDITOR_EXPORT SynthToolTrust {
    public:
        /// Returns the path of the synth tool \a value, resolved against \a utau if relative.
        /// Returns an empty path if \a value is empty.
        static std::filesystem::path resolved(const QString &value,
                                              const std::filesystem::path &utau);

        /// Returns whether the synth tool \a value is a regular file.
        static bool exists(const QString &value, const std::filesystem::path &utau);

        /// Returns whether \a first and \a second resolve to the same file.
        static bool samePath(const QString &first, const QString &second,
                             const std::filesystem::path &utau);

        /// Returns whether \a settings records trust in the synth tool \a value with the current
        /// content of its file.
        static bool isTrusted(const AppSettings &settings, const QString &value,
                              const std::filesystem::path &utau);

        /// Returns the file of the synth tool \a value and the SHA-256 with which \a settings
        /// records trust in it, if the synth tool may run only because of that trust: it is no
        /// synth tool of \a settings, and its current content is trusted. A renderer holds the
        /// file unchanged while the synth tool runs, see kit::SynthTools::trusted.
        ///
        /// \return the file, or \c std::nullopt if the synth tool runs without trust or is not
        ///         trusted
        static std::optional<kit::FileGuard::File> trustedFile(const AppSettings &settings,
                                                               const QString &value,
                                                               const std::filesystem::path &utau);

        /// Returns whether the synth tool \a value may run: it exists, and it is a synth tool of
        /// \a settings or \a settings records trust in it.
        static bool isAllowed(const AppSettings &settings, const QString &value,
                              const std::filesystem::path &utau);

        /// Shows one message box in which the user trusts or declines the synth tools of \a values
        /// that exist and are not allowed, and records the trust if the user agrees. A synth tool
        /// that cannot be trusted (refusalOf()) is reported with the reason instead, and nothing
        /// is trusted. Returns whether every synth tool of \a values is allowed afterwards.
        /// Returns \c false without the message box if a synth tool does not exist.
        static bool ask(QWidget *parent, AppSettings &settings, const QStringList &values,
                        const std::filesystem::path &utau);

        /// Returns the reason why the synth tool \a value cannot be trusted, or \c std::nullopt
        /// if it can: its file cannot be read, or users other than the current one can replace
        /// it (kit::FilePermissions::writableByOthers()).
        static std::optional<QString> refusalOf(const QString &value,
                                                const std::filesystem::path &utau);

        /// Records trust in the synth tool \a value with the current content of its file, in
        /// place of an earlier record of the same path. The records of the files that no longer
        /// exist in a folder that exists are removed, while a record in a folder that cannot be
        /// reached, such as on a disconnected drive, is kept.
        ///
        /// \return whether the trust is recorded, which requires the file to be readable
        static bool trust(AppSettings &settings, const QString &value,
                          const std::filesystem::path &utau);
    };

}

#endif // HELLOUTAU_EDITOR_SYNTHTOOLTRUST_H
