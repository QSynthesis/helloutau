#ifndef HELLOUTAU_EDITOR_SETTINGS_VIEWSETTINGS_H
#define HELLOUTAU_EDITOR_SETTINGS_VIEWSETTINGS_H

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    class AppSettingsStore;

    /// The view of the project windows, the group \c view of the settings file.
    class HELLOUTAU_EDITOR_EXPORT ViewSettings {
    public:
        ~ViewSettings();

        /// The editing grid in ticks, or zero for no grid. The default is 120.
        int quantization() const;
        void setQuantization(int ticks);

        /// Whether the piano roll shows the pitch curves (View > Show Pitch). The default is
        /// true.
        bool isPitchVisible() const;
        void setPitchVisible(bool visible);

        /// Whether the piano roll shows the pitch that the resampler receives (View > Show
        /// Rendered Pitch). The default is true.
        bool isRenderedPitchVisible() const;
        void setRenderedPitchVisible(bool visible);

        /// Whether the piano roll shows the envelopes (View > Show Envelopes). The default is
        /// true.
        bool areEnvelopesVisible() const;
        void setEnvelopesVisible(bool visible);

        /// Whether the piano roll shows the modulation and the flags of the notes (View > Show
        /// Parameters). The default is false.
        bool areParametersVisible() const;
        void setParametersVisible(bool visible);

        /// Whether the project window shows its tool bar (View > Show Toolbar). The default is
        /// true.
        bool isToolBarVisible() const;
        void setToolBarVisible(bool visible);

    private:
        explicit ViewSettings(AppSettingsStore &store);

        AppSettingsStore &m_store;

        friend class AppSettings;
        Q_DISABLE_COPY(ViewSettings)
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGS_VIEWSETTINGS_H
