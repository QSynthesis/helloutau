#ifndef HELLOUTAU_EDITOR_SETTINGS_AUDIOSETTINGS_H
#define HELLOUTAU_EDITOR_SETTINGS_AUDIOSETTINGS_H

#include <QtCore/QByteArray>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    class AppSettingsStore;

    /// The audio output, the group \c audio of the settings file.
    class HELLOUTAU_EDITOR_EXPORT AudioSettings {
    public:
        ~AudioSettings();

        /// The ID of the output device, or an empty ID for the default device of the system.
        QByteArray outputDevice() const;
        void setOutputDevice(const QByteArray &id);

    private:
        explicit AudioSettings(AppSettingsStore &store);

        AppSettingsStore &m_store;

        friend class AppSettings;
        Q_DISABLE_COPY(AudioSettings)
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGS_AUDIOSETTINGS_H
