#include "VoiceBankSettings.h"

#include <system_error>

#include <QtCore/QCoreApplication>

#include "AppSettings_p.h"

namespace hello::daw {

    namespace {

        constexpr char KeyRelativeToUtau[] = "voiceBanks/relativeToUtau";

    }

    VoiceBankSettings::VoiceBankSettings(const AppSettings &settings, AppSettingsStore &store)
        : m_settings(settings), m_store(store) {
    }

    VoiceBankSettings::~VoiceBankSettings() = default;

    std::filesystem::path VoiceBankSettings::folder() const {
        return m_settings.classicExtensionsDirectory() / u"voice";
    }

    bool VoiceBankSettings::isRelativeToUtau() const {
        return m_store.value(KeyRelativeToUtau).toBool(true);
    }

    void VoiceBankSettings::setRelativeToUtau(bool relative) {
        m_store.setValue(KeyRelativeToUtau, relative);
    }

    kit::VoiceLocations VoiceBankSettings::locations() const {
        kit::VoiceLocations locations;
        locations.dataFolders.push_back(m_settings.classicExtensionsDirectory());
        if (const auto data = UtauSettings::dataDirectory(); !data.empty()) {
            locations.dataFolders.push_back(data);
        }
        const auto utau = m_settings.utau().directory();
        std::error_code error;
        const bool utauExists = !utau.empty() && std::filesystem::is_directory(utau, error);
        if (utauExists) {
            locations.voiceFolders.push_back(utau / u"voice");
        }
        locations.relativeBase =
            utauExists && isRelativeToUtau()
                ? utau
                : std::filesystem::path(QCoreApplication::applicationDirPath().toStdU16String());
        return locations;
    }

}
