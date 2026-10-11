#include "UtauSettings.h"

#include "AppSettings_p.h"

namespace hello::daw {

    UtauSettings::UtauSettings(AppSettingsStore &store) : m_store(store) {
    }

    UtauSettings::~UtauSettings() = default;

    std::filesystem::path UtauSettings::directory() const {
        return std::filesystem::path(m_store.text(AppSettingsKeys::utauDirectory).toStdU16String());
    }

    void UtauSettings::setDirectory(const std::filesystem::path &directory) {
        m_store.setValue(AppSettingsKeys::utauDirectory,
                         AppSettingsStore::pathText(directory).toStdString());
    }

}
