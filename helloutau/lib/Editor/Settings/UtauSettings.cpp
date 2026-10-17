#include "UtauSettings.h"

#include <stdcorelib/support/systempaths.h>

#include "AppSettings_p.h"

namespace hello::daw {

    namespace {

        std::optional<std::filesystem::path> &replacedDataDirectory() {
            static std::optional<std::filesystem::path> directory;
            return directory;
        }

    }

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

    std::filesystem::path UtauSettings::dataDirectory() {
        if (const auto &replaced = replacedDataDirectory()) {
            return *replaced;
        }
        using stdc::system::SystemPaths;
        if (const auto appData = SystemPaths::writableDirectory(SystemPaths::AppDataDirectory)) {
            return *appData / u"UTAU";
        }
        return {};
    }

    void UtauSettings::setDataDirectory(const std::optional<std::filesystem::path> &directory) {
        replacedDataDirectory() = directory;
    }

}
