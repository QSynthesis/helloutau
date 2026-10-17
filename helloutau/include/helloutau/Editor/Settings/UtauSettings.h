#ifndef HELLOUTAU_EDITOR_SETTINGS_UTAUSETTINGS_H
#define HELLOUTAU_EDITOR_SETTINGS_UTAUSETTINGS_H

#include <filesystem>
#include <optional>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    class AppSettingsStore;

    /// The installation of UTAU, the group \c utau of the settings file.
    class HELLOUTAU_EDITOR_EXPORT UtauSettings {
    public:
        ~UtauSettings();

        /// Directory that contains \c utau.exe, or an empty path if not set.
        ///
        /// The directory serves four purposes. A relative \c Tool1 or \c Tool2 is resolved
        /// against it. A relative \c VoiceDir is resolved against it if
        /// VoiceBankSettings::isRelativeToUtau() is true. Its \c voice directory is the voice
        /// folder of VoiceBankSettings::locations(), which \c %VOICE% denotes, and its \c plugins
        /// directory holds the plugins of UTAU.
        std::filesystem::path directory() const;
        void setDirectory(const std::filesystem::path &directory);

        /// Returns the folder into which the installer of UTAU puts voice banks and plugins, which
        /// UTAU denotes by \c %DATA%: the \c UTAU directory of the application data folder of the
        /// user, the roaming one on Windows. Other systems have no native UTAU, and the folder of
        /// the same name serves there for the voice banks and plugins copied from Windows.
        /// Returns an empty path if the system reports no application data folder. The folder is
        /// not created.
        static std::filesystem::path dataDirectory();

        /// Replaces the result of dataDirectory() with \a directory, for the tests that must not
        /// see the voice banks and plugins of the user. \c std::nullopt restores the folder of
        /// the system.
        ///
        /// \warning Not thread-safe.
        static void setDataDirectory(const std::optional<std::filesystem::path> &directory);

    private:
        explicit UtauSettings(AppSettingsStore &store);

        AppSettingsStore &m_store;

        friend class AppSettings;
        Q_DISABLE_COPY(UtauSettings)
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGS_UTAUSETTINGS_H
