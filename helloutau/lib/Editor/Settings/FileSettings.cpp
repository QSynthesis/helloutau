#include "FileSettings.h"

#include "AppSettings_p.h"

namespace hello::daw {

    namespace {

        constexpr char KeyUstExportCharset[] = "files/ustExportCharset";

        QList<std::filesystem::path> pathsOf(const QStringList &texts) {
            QList<std::filesystem::path> paths;
            for (const auto &text : texts) {
                paths.push_back(std::filesystem::path(text.toStdU16String()));
            }
            return paths;
        }

    }

    FileSettings::FileSettings(AppSettingsStore &store) : m_store(store) {
    }

    FileSettings::~FileSettings() = default;

    QString FileSettings::ustExportCharset() const {
        const auto charset = m_store.value(KeyUstExportCharset).asString();
        return charset ? QString::fromStdString(*charset) : QStringLiteral("UTF-8");
    }

    void FileSettings::setUstExportCharset(const QString &charset) {
        m_store.setValue(KeyUstExportCharset, charset.toStdString());
    }

    QList<std::filesystem::path> FileSettings::recentFiles() const {
        return pathsOf(
            AppSettingsStore::recentTextsOf(m_store.value(AppSettingsKeys::recentFiles)));
    }

    void FileSettings::addRecentFile(const std::filesystem::path &path) {
        auto texts = AppSettingsStore::recentTextsOf(m_store.value(AppSettingsKeys::recentFiles));
        const auto normalized = AppSettingsStore::pathText(path);
        texts.removeAll(normalized);
        texts.prepend(normalized);
        m_store.setValue(AppSettingsKeys::recentFiles,
                         AppSettingsStore::arrayOf(texts.mid(0, recentFileCount)));
    }

    void FileSettings::removeRecentFile(const std::filesystem::path &path) {
        auto texts = AppSettingsStore::recentTextsOf(m_store.value(AppSettingsKeys::recentFiles));
        texts.removeAll(AppSettingsStore::pathText(path));
        m_store.setValue(AppSettingsKeys::recentFiles, AppSettingsStore::arrayOf(texts));
    }

    void FileSettings::clearRecentFiles() {
        m_store.setValue(AppSettingsKeys::recentFiles, {});
    }

    QList<std::filesystem::path> FileSettings::recentVoiceBanks() const {
        return pathsOf(
            AppSettingsStore::recentTextsOf(m_store.value(AppSettingsKeys::recentVoiceBanks)));
    }

    void FileSettings::addRecentVoiceBank(const std::filesystem::path &root) {
        auto texts =
            AppSettingsStore::recentTextsOf(m_store.value(AppSettingsKeys::recentVoiceBanks));
        const auto normalized = AppSettingsStore::pathText(root);
        texts.removeAll(normalized);
        texts.prepend(normalized);
        m_store.setValue(AppSettingsKeys::recentVoiceBanks,
                         AppSettingsStore::arrayOf(texts.mid(0, recentFileCount)));
    }

    void FileSettings::removeRecentVoiceBank(const std::filesystem::path &root) {
        auto texts =
            AppSettingsStore::recentTextsOf(m_store.value(AppSettingsKeys::recentVoiceBanks));
        texts.removeAll(AppSettingsStore::pathText(root));
        m_store.setValue(AppSettingsKeys::recentVoiceBanks, AppSettingsStore::arrayOf(texts));
    }

    void FileSettings::clearRecentVoiceBanks() {
        m_store.setValue(AppSettingsKeys::recentVoiceBanks, {});
    }

}
