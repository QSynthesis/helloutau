#include "AppSettings.h"

#include <QtCore/QCoreApplication>

namespace hello::daw {

    namespace {

        constexpr char KeyUtauDirectory[] = "engines/utauDirectory";
        constexpr char KeyResampler[] = "engines/resampler";
        constexpr char KeyWavtool[] = "engines/wavtool";
        constexpr char KeyUstExportCharset[] = "files/ustExportCharset";
        constexpr char KeyRecentCommands[] = "commandPalette/recent";

    }

    AppSettings::AppSettings()
        : m_settings(std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope,
                                                 QCoreApplication::organizationName(),
                                                 QCoreApplication::applicationName())) {
    }

    AppSettings::AppSettings(const QString &fileName)
        : m_settings(std::make_unique<QSettings>(fileName, QSettings::IniFormat)) {
    }

    AppSettings::~AppSettings() = default;

    std::filesystem::path AppSettings::utauDirectory() const {
        return std::filesystem::path(
            m_settings->value(QLatin1String(KeyUtauDirectory)).toString().toStdU16String());
    }

    void AppSettings::setUtauDirectory(const std::filesystem::path &directory) {
        m_settings->setValue(QLatin1String(KeyUtauDirectory),
                             QString::fromStdU16String(directory.u16string()));
    }

    QString AppSettings::resampler() const {
        return m_settings->value(QLatin1String(KeyResampler)).toString();
    }

    void AppSettings::setResampler(const QString &path) {
        m_settings->setValue(QLatin1String(KeyResampler), path);
    }

    QString AppSettings::wavtool() const {
        return m_settings->value(QLatin1String(KeyWavtool)).toString();
    }

    void AppSettings::setWavtool(const QString &path) {
        m_settings->setValue(QLatin1String(KeyWavtool), path);
    }

    QString AppSettings::ustExportCharset() const {
        return m_settings->value(QLatin1String(KeyUstExportCharset), QStringLiteral("UTF-8"))
            .toString();
    }

    void AppSettings::setUstExportCharset(const QString &charset) {
        m_settings->setValue(QLatin1String(KeyUstExportCharset), charset);
    }

    QStringList AppSettings::recentCommands() const {
        return m_settings->value(QLatin1String(KeyRecentCommands)).toStringList();
    }

    void AppSettings::addRecentCommand(const QString &id) {
        auto ids = recentCommands();
        ids.removeAll(id);
        ids.prepend(id);
        m_settings->setValue(QLatin1String(KeyRecentCommands), ids.mid(0, recentCommandCount));
    }

}
