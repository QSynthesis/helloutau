#ifndef HELLOUTAU_EDITOR_SETTINGS_FREQUENCYMAPSETTINGS_H
#define HELLOUTAU_EDITOR_SETTINGS_FREQUENCYMAPSETTINGS_H

#include <QtCore/QtGlobal>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    class AppSettingsStore;

    /// The generation of frequency maps in a voice bank window, the group \c frequencyMaps of
    /// the settings file. See the generation in docs/FrequencyMaps.md.
    class HELLOUTAU_EDITOR_EXPORT FrequencyMapSettings {
    public:
        ~FrequencyMapSettings();

        /// The number of resamplers that generate maps at once. Zero, the default, stands for
        /// one per hardware thread. Independent of RenderingSettings::threadCount(), which the
        /// classic playback does not use.
        int threadCount() const;
        void setThreadCount(int count);

        /// What generating the maps again removes first.
        enum Regeneration {
            MapsAndCaches, ///< The maps and their analysis caches, the default
            CachesOnly,    ///< The analysis caches only, which the resampler writes from the maps
        };

        /// What generating the maps again removes first. A format without analysis caches
        /// removes its maps regardless.
        Regeneration regeneration() const;
        void setRegeneration(Regeneration regeneration);

    private:
        explicit FrequencyMapSettings(AppSettingsStore &store);

        AppSettingsStore &m_store;

        friend class AppSettings;
        Q_DISABLE_COPY(FrequencyMapSettings)
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGS_FREQUENCYMAPSETTINGS_H
