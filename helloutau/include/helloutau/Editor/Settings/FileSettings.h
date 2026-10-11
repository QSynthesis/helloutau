#ifndef HELLOUTAU_EDITOR_SETTINGS_FILESETTINGS_H
#define HELLOUTAU_EDITOR_SETTINGS_FILESETTINGS_H

#include <filesystem>

#include <QtCore/QList>
#include <QtCore/QString>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    class AppSettingsStore;

    /// The export of USTs and the recent files, the group \c files of the settings file.
    class HELLOUTAU_EDITOR_EXPORT FileSettings {
    public:
        ~FileSettings();

        /// Encoding initially selected when a UST is exported. The default is UTF-8.
        QString ustExportCharset() const;
        void setUstExportCharset(const QString &charset);

        /// Returns the project files last opened or saved as, most recent first. The list is
        /// shared by all windows.
        QList<std::filesystem::path> recentFiles() const;

        /// Places \a path first among the recent files. At most \c recentFileCount files are
        /// stored.
        void addRecentFile(const std::filesystem::path &path);
        void removeRecentFile(const std::filesystem::path &path);
        void clearRecentFiles();

        /// Returns the folders of the voice banks last opened or saved as, most recent first. The
        /// list is shared by all windows.
        QList<std::filesystem::path> recentVoiceBanks() const;

        /// Places \a root first among the recent voice banks. At most \c recentFileCount voice
        /// banks are stored.
        void addRecentVoiceBank(const std::filesystem::path &root);
        void removeRecentVoiceBank(const std::filesystem::path &root);
        void clearRecentVoiceBanks();

        /// Maximum number of stored recent files, and of stored recent voice banks. The menu
        /// "Open Recent" shows fewer entries, and "More..." shows all of them.
        static constexpr int recentFileCount = 50;

    private:
        explicit FileSettings(AppSettingsStore &store);

        AppSettingsStore &m_store;

        friend class AppSettings;
        Q_DISABLE_COPY(FileSettings)
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGS_FILESETTINGS_H
