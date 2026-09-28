#ifndef HELLOUTAU_EDITOR_APPSETTINGS_H
#define HELLOUTAU_EDITOR_APPSETTINGS_H

#include <filesystem>
#include <memory>

#include <QtCore/QList>
#include <QtCore/QSettings>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    /// The settings of the editor, stored per user in an INI file.
    class HELLOUTAU_EDITOR_EXPORT AppSettings {
    public:
        /// The settings of the current user, in the location Qt chooses for the organization and
        /// application names of \c QCoreApplication.
        AppSettings();

        /// The settings stored in \a fileName.
        explicit AppSettings(const QString &fileName);

        ~AppSettings();

        /// The directory that contains \c utau.exe, which resolves the \c %VOICE% prefix and
        /// relative paths in \c VoiceDir, see Track::voiceDirectory(). Empty if not set.
        std::filesystem::path utauDirectory() const;
        void setUtauDirectory(const std::filesystem::path &directory);

        /// The engines used for rendering and written to an exported UST that specifies none.
        /// The engines a project specifies are never used without asking, see AGENTS.md.
        QString resampler() const;
        void setResampler(const QString &path);
        QString wavtool() const;
        void setWavtool(const QString &path);

        /// The encoding initially selected when a UST is exported, UTF-8 by default.
        QString ustExportCharset() const;
        void setUstExportCharset(const QString &charset);

        /// The commands last chosen in the command palette, the latest first, shared by all
        /// windows.
        QStringList recentCommands() const;

        /// Puts \a id first among the recent commands. At most \c recentCommandCount are kept.
        void addRecentCommand(const QString &id);

        static constexpr int recentCommandCount = 20;

        /// The files last opened or saved as, the latest first, shared by all windows.
        QList<std::filesystem::path> recentFiles() const;

        /// Puts \a path first among the recent files. At most \c recentFileCount are kept.
        void addRecentFile(const std::filesystem::path &path);
        void removeRecentFile(const std::filesystem::path &path);
        void clearRecentFiles();

        static constexpr int recentFileCount = 10;

    private:
        Q_DISABLE_COPY(AppSettings)

        std::unique_ptr<QSettings> m_settings;
    };

}

#endif // HELLOUTAU_EDITOR_APPSETTINGS_H
