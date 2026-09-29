#include "AppSettings.h"

#include <QtCore/QCoreApplication>

namespace hello::daw {

    namespace {

        constexpr char KeyUtauDirectory[] = "engines/utauDirectory";
        constexpr char KeyResampler[] = "engines/resampler";
        constexpr char KeyWavtool[] = "engines/wavtool";
        constexpr char KeyPlaybackMode[] = "playback/mode";
        constexpr char PrerenderValue[] = "prerender";
        constexpr char RealtimeValue[] = "realtime";
        constexpr char KeyUstExportCharset[] = "files/ustExportCharset";
        constexpr char KeyRecentCommands[] = "commandPalette/recent";
        constexpr char KeyRecentFiles[] = "files/recent";
        constexpr char KeyRecentVoiceBanks[] = "files/recentVoiceBanks";

        // A path as the settings keep it, whole and in UTF-16
        QString textOf(const std::filesystem::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

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

    AppSettings::PlaybackMode AppSettings::playbackMode() const {
        return m_settings->value(QLatin1String(KeyPlaybackMode)).toString() ==
                       QLatin1String(RealtimeValue)
                   ? Realtime
                   : Prerender;
    }

    void AppSettings::setPlaybackMode(PlaybackMode mode) {
        m_settings->setValue(QLatin1String(KeyPlaybackMode),
                             QLatin1String(mode == Realtime ? RealtimeValue : PrerenderValue));
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

    QList<std::filesystem::path> AppSettings::recentPaths(const char *key) const {
        QList<std::filesystem::path> paths;
        for (const auto &text : m_settings->value(QLatin1String(key)).toStringList()) {
            paths.push_back(std::filesystem::path(text.toStdU16String()));
        }
        return paths;
    }

    void AppSettings::addRecentPath(const char *key, const std::filesystem::path &path) {
        auto texts = m_settings->value(QLatin1String(key)).toStringList();
        const auto text = textOf(path);
        texts.removeAll(text);
        texts.prepend(text);
        m_settings->setValue(QLatin1String(key), texts.mid(0, recentFileCount));
    }

    void AppSettings::removeRecentPath(const char *key, const std::filesystem::path &path) {
        auto texts = m_settings->value(QLatin1String(key)).toStringList();
        texts.removeAll(textOf(path));
        m_settings->setValue(QLatin1String(key), texts);
    }

    QList<std::filesystem::path> AppSettings::recentFiles() const {
        return recentPaths(KeyRecentFiles);
    }

    void AppSettings::addRecentFile(const std::filesystem::path &path) {
        addRecentPath(KeyRecentFiles, path);
    }

    void AppSettings::removeRecentFile(const std::filesystem::path &path) {
        removeRecentPath(KeyRecentFiles, path);
    }

    void AppSettings::clearRecentFiles() {
        m_settings->remove(QLatin1String(KeyRecentFiles));
    }

    QList<std::filesystem::path> AppSettings::recentVoiceBanks() const {
        return recentPaths(KeyRecentVoiceBanks);
    }

    void AppSettings::addRecentVoiceBank(const std::filesystem::path &root) {
        addRecentPath(KeyRecentVoiceBanks, root);
    }

    void AppSettings::removeRecentVoiceBank(const std::filesystem::path &root) {
        removeRecentPath(KeyRecentVoiceBanks, root);
    }

    void AppSettings::clearRecentVoiceBanks() {
        m_settings->remove(QLatin1String(KeyRecentVoiceBanks));
    }

}
