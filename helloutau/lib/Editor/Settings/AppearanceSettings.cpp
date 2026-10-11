#include "AppearanceSettings.h"

#include "AppSettings_p.h"

namespace hello::daw {

    namespace {

        constexpr char KeyLanguage[] = "appearance/language";

    }

    AppearanceSettings::AppearanceSettings(AppSettingsStore &store) : m_store(store) {
    }

    AppearanceSettings::~AppearanceSettings() = default;

    QString AppearanceSettings::language() const {
        const auto language = m_store.value(KeyLanguage).asString();
        return language ? QString::fromStdString(*language) : QString();
    }

    void AppearanceSettings::setLanguage(const QString &language) {
        m_store.setValue(KeyLanguage, language.toStdString());
    }

    QList<std::pair<QString, QString>> AppearanceSettings::languages() {
        return {
            {QString(),               tr("System Default")      },
            {QStringLiteral("en"),    QStringLiteral("English") },
            {QStringLiteral("zh_CN"), QStringLiteral("简体中文")},
        };
    }

    QLocale AppearanceSettings::localeOf(const QString &language) {
        return language.isEmpty() ? QLocale::system() : QLocale(language);
    }

}
