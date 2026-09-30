#ifndef HELLOUTAU_EDITOR_APPSETTINGS_H
#define HELLOUTAU_EDITOR_APPSETTINGS_H

#include <filesystem>
#include <memory>

#include <QtCore/QJsonValue>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    /// The settings of the application, stored per user in a JSON file. Those of the plugins are
    /// kept apart, see AppLoader.
    ///
    /// The file is read once. It is written whole after the changes, once the event loop runs,
    /// so that the changes of one pass of the loop make one write, and by sync() and the
    /// destructor. Of two applications that change it, the one that writes last prevails.
    class HELLOUTAU_EDITOR_EXPORT AppSettings {
    public:
        /// The settings of the current user, in defaultFileName().
        AppSettings();

        /// The settings stored in \a fileName.
        explicit AppSettings(const QString &fileName);

        ~AppSettings();

        /// The directory of the settings of the current user: the data directory that Qt
        /// chooses for the organization and application names of \c QCoreApplication.
        static QString defaultDirectory();

        /// \c settings.json in defaultDirectory().
        static QString defaultFileName();

        QString fileName() const;

        /// Writes the changes that are pending now.
        void sync();

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

        /// The manner in which a project plays, see the playback modes in docs/Widgets.md.
        enum PlaybackMode {
            /// The selected notes are rendered by \c temp.bat in a console, as UTAU renders
            /// them, and then played.
            Prerender,

            /// The track is rendered in the background, from the playhead first, and plays from
            /// the playhead at once.
            Realtime,
        };

        /// Prerender by default, as UTAU plays.
        PlaybackMode playbackMode() const;
        void setPlaybackMode(PlaybackMode mode);

        /// The encoding initially selected when a UST is exported, UTF-8 by default.
        QString ustExportCharset() const;
        void setUstExportCharset(const QString &charset);

        /// The commands last chosen in the command palette, the latest first, shared by all
        /// windows.
        QStringList recentCommands() const;

        /// Puts \a id first among the recent commands. At most \c recentCommandCount are kept.
        void addRecentCommand(const QString &id);

        static constexpr int recentCommandCount = 20;

        /// The project files last opened or saved as, the latest first, shared by all windows.
        QList<std::filesystem::path> recentFiles() const;

        /// Puts \a path first among the recent files. At most \c recentFileCount are kept.
        void addRecentFile(const std::filesystem::path &path);
        void removeRecentFile(const std::filesystem::path &path);
        void clearRecentFiles();

        /// The folders of the voice banks last opened or saved as, the latest first, shared by
        /// all windows.
        QList<std::filesystem::path> recentVoiceBanks() const;

        /// Puts \a root first among the recent voice banks. At most \c recentFileCount are kept.
        void addRecentVoiceBank(const std::filesystem::path &root);
        void removeRecentVoiceBank(const std::filesystem::path &root);
        void clearRecentVoiceBanks();

        /// How many recent files and voice banks are kept, each. The menu "Open Recent" shows
        /// fewer, and "More..." all of them.
        static constexpr int recentFileCount = 50;

        /// \name Values by key
        ///
        /// Any value of the file, by its key: the names of its groups and its own, joined by
        /// slashes, such as \c engines/resampler. The functions above are these for the values
        /// of the application.
        /// @{

        /// The value at \a key, undefined if there is none.
        QJsonValue value(const QString &key) const;

        /// Replaces the value at \a key, creating the groups it lies in, and writes the file.
        /// An undefined or null \a value removes it, and with it each group that it leaves
        /// empty.
        void setValue(const QString &key, const QJsonValue &value);
        /// @}

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;

        Q_DISABLE_COPY(AppSettings)
    };

}

#endif // HELLOUTAU_EDITOR_APPSETTINGS_H
