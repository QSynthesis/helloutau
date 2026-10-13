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

        /// The parts that generating the maps again removes first.
        enum Regeneration {
            MapsAndCaches, ///< The maps and their analysis caches, the default
            CachesOnly,    ///< The analysis caches only, which the resampler writes from the maps
        };

        /// The parts that generating the maps again removes first. A format without analysis
        /// caches removes its maps regardless.
        Regeneration regeneration() const;
        void setRegeneration(Regeneration regeneration);

        /// Returns whether the runs held in the log of the generation and its size limit are
        /// those of the render log, the default, rather than isLogAccumulated() and logLimit().
        bool followsRenderLog() const;
        void setFollowsRenderLog(bool follows);

        /// Returns whether the log of the generation holds every run rather than the latest,
        /// unless followsRenderLog(). Every run by default.
        bool isLogAccumulated() const;
        void setLogAccumulated(bool accumulated);

        /// The size limit of the log of the generation in bytes, unless followsRenderLog(). The
        /// default is 1 MiB.
        int logLimit() const;
        void setLogLimit(int bytes);

    private:
        explicit FrequencyMapSettings(AppSettingsStore &store);

        AppSettingsStore &m_store;

        friend class AppSettings;
        Q_DISABLE_COPY(FrequencyMapSettings)
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGS_FREQUENCYMAPSETTINGS_H
