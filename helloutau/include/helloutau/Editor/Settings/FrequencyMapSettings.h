#ifndef HELLOUTAU_EDITOR_SETTINGS_FREQUENCYMAPSETTINGS_H
#define HELLOUTAU_EDITOR_SETTINGS_FREQUENCYMAPSETTINGS_H

#include <QtCore/QtGlobal>

#include <hellokit/VoiceBank/FrequencyMapCheck.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    class AppSettingsStore;

    /// The generation and the editing of frequency maps in a voice bank window, the group
    /// \c frequencyMaps of the settings file. See the generation and the editing in
    /// docs/FrequencyMaps.md.
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

        /// Returns whether saving an edited map removes the analysis caches of its format, the
        /// default. The resampler reads a cache in place of the map, therefore an edit takes
        /// effect in synthesis only after the cache is removed.
        bool removesCachesOnSave() const;
        void setRemovesCachesOnSave(bool removes);

        /// \name The editing area
        /// The state that the editing dialog last left, which the next dialog starts from. The
        /// zoom is not kept.
        /// @{

        /// Returns whether the editing area shows the spectrogram rather than the rows of the
        /// keys. The rows by default.
        bool showsSpectrogram() const;
        void setShowsSpectrogram(bool shows);

        /// Returns the brightness of the spectrogram, FrequencyMapView::brightness(), limited to
        /// its range. 0 by default.
        int spectrogramBrightness() const;
        void setSpectrogramBrightness(int brightness);

        /// Returns the color scheme of the spectrogram, FrequencyMapView::colorScheme(), limited
        /// to its range. 0 by default.
        int spectrogramColorScheme() const;
        void setSpectrogramColorScheme(int scheme);

        /// Returns the settings that the check of the editing dialog last used: by default C4,
        /// with no option. The note is limited to C1 to B7, as frqeditor offers it.
        kit::FrequencyMapCheck::Settings checkSettings() const;
        void setCheckSettings(const kit::FrequencyMapCheck::Settings &settings);
        /// @}

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
