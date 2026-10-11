#ifndef HELLOUTAU_EDITOR_SETTINGS_PITCHCONTROLSETTINGS_H
#define HELLOUTAU_EDITOR_SETTINGS_PITCHCONTROLSETTINGS_H

#include <hellokit/Document/PortamentoSettings.h>
#include <hellokit/Document/Project.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    class AppSettingsStore;

    /// The defaults of the Pitch Control dialog, the group \c pitchControl of the settings file.
    class HELLOUTAU_EDITOR_EXPORT PitchControlSettings {
    public:
        ~PitchControlSettings();

        /// The settings of the portamento group of Pitch Control, written as a whole by its Set
        /// as Default. The default is that of kit::PortamentoSettings.
        kit::PortamentoSettings portamento() const;
        void setPortamento(const kit::PortamentoSettings &settings);

        /// The vibrato of Pitch Control for notes without one, and the index of its vibrato
        /// preset, written by its Set as Default. The defaults are kit::Vibrato::utauDefault()
        /// and the first preset.
        kit::Vibrato vibrato() const;
        void setVibrato(const kit::Vibrato &vibrato);
        int vibratoPreset() const;
        void setVibratoPreset(int index);

    private:
        explicit PitchControlSettings(AppSettingsStore &store);

        AppSettingsStore &m_store;

        friend class AppSettings;
        Q_DISABLE_COPY(PitchControlSettings)
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGS_PITCHCONTROLSETTINGS_H
