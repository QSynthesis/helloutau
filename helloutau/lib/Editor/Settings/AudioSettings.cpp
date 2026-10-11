#include "AudioSettings.h"

#include "AppSettings_p.h"

namespace hello::daw {

    namespace {

        constexpr char KeyOutputDevice[] = "audio/outputDevice";

    }

    AudioSettings::AudioSettings(AppSettingsStore &store) : m_store(store) {
    }

    AudioSettings::~AudioSettings() = default;

    QByteArray AudioSettings::outputDevice() const {
        return QByteArray::fromBase64(
            QByteArray::fromStdString(m_store.value(KeyOutputDevice).toString()));
    }

    void AudioSettings::setOutputDevice(const QByteArray &id) {
        m_store.setValue(KeyOutputDevice, id.toBase64().toStdString());
    }

}
