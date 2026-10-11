#ifndef HELLOUTAU_EDITOR_SETTINGS_UTAUSETTINGS_H
#define HELLOUTAU_EDITOR_SETTINGS_UTAUSETTINGS_H

#include <filesystem>

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
        /// VoiceBankSettings::isRelativeToUtau() is true. Its \c voice directory is the second
        /// voice folder of VoiceBankSettings::locations(), and its \c plugins directory holds the
        /// plugins of UTAU.
        std::filesystem::path directory() const;
        void setDirectory(const std::filesystem::path &directory);

    private:
        explicit UtauSettings(AppSettingsStore &store);

        AppSettingsStore &m_store;

        friend class AppSettings;
        Q_DISABLE_COPY(UtauSettings)
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGS_UTAUSETTINGS_H
