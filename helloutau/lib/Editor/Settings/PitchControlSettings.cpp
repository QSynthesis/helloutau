#include "PitchControlSettings.h"

#include <algorithm>

#include <hellokit/Support/JsonInterop.h>

#include "AppSettings_p.h"

namespace hello::daw {

    namespace {

        constexpr char KeyPortamento[] = "pitchControl/portamento";
        constexpr char KeyVibrato[] = "pitchControl/vibrato";
        constexpr char KeyVibratoPreset[] = "pitchControl/vibratoPreset";

    }

    PitchControlSettings::PitchControlSettings(AppSettingsStore &store) : m_store(store) {
    }

    PitchControlSettings::~PitchControlSettings() = default;

    // The values are objects of the document library, which reads and writes them as Qt JSON.

    kit::PortamentoSettings PitchControlSettings::portamento() const {
        return kit::PortamentoSettings::fromJson(
            kit::JsonInterop::toQtJson(m_store.value(KeyPortamento)).toObject());
    }

    void PitchControlSettings::setPortamento(const kit::PortamentoSettings &settings) {
        m_store.setValue(KeyPortamento, kit::JsonInterop::fromQtJson(settings.toJson()));
    }

    kit::Vibrato PitchControlSettings::vibrato() const {
        const auto stored = kit::JsonInterop::toQtJson(m_store.value(KeyVibrato));
        return stored.isObject() ? kit::Vibrato::fromJson(stored.toObject())
                                 : kit::Vibrato::utauDefault();
    }

    void PitchControlSettings::setVibrato(const kit::Vibrato &vibrato) {
        m_store.setValue(KeyVibrato, kit::JsonInterop::fromQtJson(vibrato.toJson()));
    }

    int PitchControlSettings::vibratoPreset() const {
        return std::max(0, int(m_store.value(KeyVibratoPreset).toInt(0)));
    }

    void PitchControlSettings::setVibratoPreset(int index) {
        m_store.setValue(KeyVibratoPreset, int64_t(std::max(0, index)));
    }

}
