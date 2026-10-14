#ifndef HELLOUTAU_EDITOR_SETTINGS_TASKSETTINGS_H
#define HELLOUTAU_EDITOR_SETTINGS_TASKSETTINGS_H

#include <QtCore/QtGlobal>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    class AppSettingsStore;

    /// The general background tasks of the editor, such as reading the properties of the audio
    /// files of a voice bank before they are processed, the group \c tasks of the settings file.
    class HELLOUTAU_EDITOR_EXPORT TaskSettings {
    public:
        ~TaskSettings();

        /// The number of threads of a general task. Zero, the default, stands for one per
        /// hardware thread. Independent of the threads of the rendering and of the frequency
        /// maps, which run synth tools.
        int threadCount() const;
        void setThreadCount(int count);

    private:
        explicit TaskSettings(AppSettingsStore &store);

        AppSettingsStore &m_store;

        friend class AppSettings;
        Q_DISABLE_COPY(TaskSettings)
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGS_TASKSETTINGS_H
