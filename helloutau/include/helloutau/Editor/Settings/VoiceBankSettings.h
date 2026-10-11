#ifndef HELLOUTAU_EDITOR_SETTINGS_VOICEBANKSETTINGS_H
#define HELLOUTAU_EDITOR_SETTINGS_VOICEBANKSETTINGS_H

#include <filesystem>

#include <hellokit/Document/Project.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    class AppSettings;
    class AppSettingsStore;

    /// The locations of the voice banks, the group \c voiceBanks of the settings file.
    class HELLOUTAU_EDITOR_EXPORT VoiceBankSettings {
    public:
        ~VoiceBankSettings();

        /// Returns the voice folder of HelloUtau, the \c Singers directory of
        /// AppSettings::userDirectory(). The folder is the first voice folder of locations(),
        /// and it is not created by this class.
        std::filesystem::path folder() const;

        /// Whether a relative \c VoiceDir is resolved against UtauSettings::directory(), as UTAU
        /// resolves it, rather than against the directory of HelloUtau. The setting applies only
        /// if the UTAU directory exists. The default is true.
        bool isRelativeToUtau() const;
        void setRelativeToUtau(bool relative);

        /// Returns the locations against which a \c VoiceDir is resolved. The \c %VOICE% prefix
        /// denotes folder() and then the \c voice directory of UtauSettings::directory(), and a
        /// relative path is resolved as isRelativeToUtau() specifies.
        kit::VoiceLocations locations() const;

    private:
        VoiceBankSettings(const AppSettings &settings, AppSettingsStore &store);

        const AppSettings &m_settings;
        AppSettingsStore &m_store;

        friend class AppSettings;
        Q_DISABLE_COPY(VoiceBankSettings)
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGS_VOICEBANKSETTINGS_H
