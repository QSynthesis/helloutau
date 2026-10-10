#ifndef HELLOUTAU_EDITOR_APPSETTINGS_H
#define HELLOUTAU_EDITOR_APPSETTINGS_H

#include <filesystem>
#include <memory>

#include <QtCore/QJsonValue>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Document/PortamentoSettings.h>
#include <hellokit/Document/Project.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    /// Settings of the application, stored per user in a JSON file. The settings of the plugins
    /// are stored separately, see AppLoader.
    ///
    /// The file is read once. After changes, the whole file is written once the event loop runs,
    /// so that the changes of one pass of the loop result in one write. sync() and the destructor
    /// also write the file. If two applications change the file, the last write prevails.
    class HELLOUTAU_EDITOR_EXPORT AppSettings {
    public:
        /// Constructs the settings of the current user, stored in defaultFileName(), with
        /// defaultUserDirectory() as the user directory.
        AppSettings();

        /// Constructs the settings stored in \a fileName, with the directory of \a fileName as
        /// the user directory, so that a settings directory other than the default one, such as
        /// that of a test, holds the voice banks and plugins of the user as well.
        explicit AppSettings(const QString &fileName);

        /// Constructs the settings stored in \a fileName with \a userDirectory as the user
        /// directory.
        AppSettings(const QString &fileName, const QString &userDirectory);

        ~AppSettings();

        /// Returns the settings directory of the current user: the data directory that Qt
        /// determines from the organization and application names of \c QCoreApplication.
        static QString defaultDirectory();

        /// Returns the path of \c settings.json in defaultDirectory().
        static QString defaultFileName();

        /// Returns the user directory of the current user: \c OpenVPI/HelloUtau, after the
        /// organization and application names of \c QCoreApplication, in the documents
        /// directory. See the user directories in docs/Distribution.md.
        static QString defaultUserDirectory();

        QString fileName() const;

        /// Returns the directory of the files that the user adds and manages, the voice banks
        /// and the plugins. The directory is not created by this class.
        std::filesystem::path userDirectory() const;

        /// Writes the pending changes immediately.
        void sync();

        /// Directory that contains \c utau.exe, or an empty path if not set.
        ///
        /// The directory serves four purposes. A relative \c Tool1 or \c Tool2 is resolved
        /// against it. A relative \c VoiceDir is resolved against it if
        /// isRelativeVoiceDirInUtau() is true. Its \c voice directory is the second voice folder
        /// of voiceLocations(), and its \c plugins directory holds the plugins of UTAU.
        std::filesystem::path utauDirectory() const;
        void setUtauDirectory(const std::filesystem::path &directory);

        /// Returns the voice folder of HelloUtau, the \c Singers directory of userDirectory().
        /// The folder is the first voice folder of voiceLocations(), and it is not created by
        /// this class.
        std::filesystem::path voiceFolder() const;

        /// Whether a relative \c VoiceDir is resolved against utauDirectory(), as UTAU resolves
        /// it, rather than against the directory of HelloUtau. The setting applies only if
        /// utauDirectory() exists. The default is true.
        bool isRelativeVoiceDirInUtau() const;
        void setRelativeVoiceDirInUtau(bool inUtau);

        /// Returns the locations against which a \c VoiceDir is resolved. The \c %VOICE% prefix
        /// denotes voiceFolder() and then the \c voice directory of utauDirectory(), and a
        /// relative path is resolved as isRelativeVoiceDirInUtau() specifies.
        kit::VoiceLocations voiceLocations() const;

        /// Synth tools used for rendering and written to an exported UST that specifies no synth
        /// tool. The synth tools specified by a project are never used without confirmation by the
        /// user, see CLAUDE.md.
        QString resampler() const;
        void setResampler(const QString &path);
        QString wavtool() const;
        void setWavtool(const QString &path);

        /// Whether the render log keeps all captured synth tool output, or only the latest run.
        bool isRenderLogAccumulated() const;
        void setRenderLogAccumulated(bool accumulated);

        /// Maximum size of the captured render log in bytes. The default is 1 MiB.
        int renderLogLimit() const;
        void setRenderLogLimit(int bytes);

        QByteArray audioOutputDevice() const;
        void setAudioOutputDevice(const QByteArray &id);

        /// Playback mode of a project, see the playback modes in docs/Widgets.md.
        enum PlaybackMode {
            /// The selected notes are rendered by \c temp.bat in a console, as in UTAU, and then
            /// played.
            Prerender,

            /// The selected notes are rendered by several threads in the process, and then
            /// played.
            ThreadedPrerender,

            /// The track is rendered in the background, starting from the playhead. Playback
            /// starts from the playhead immediately.
            Realtime,
        };

        /// Playback mode. The default is Prerender, as in UTAU.
        PlaybackMode playbackMode() const;
        void setPlaybackMode(PlaybackMode mode);

        /// Number of rendering threads of the ThreadedPrerender and Realtime modes, and of the
        /// rendering of a whole track outside the Prerender mode. Zero, the default, stands for
        /// one thread per hardware thread.
        int renderThreadCount() const;
        void setRenderThreadCount(int count);

        /// The editing grid in ticks, or zero for no grid. The default is 120.
        int quantization() const;
        void setQuantization(int ticks);

        /// The settings of the portamento group of Pitch Control, written as a whole by its Set
        /// as Default. The default is that of kit::PortamentoSettings.
        kit::PortamentoSettings pitchControlPortamento() const;
        void setPitchControlPortamento(const kit::PortamentoSettings &settings);

        /// The vibrato of Pitch Control for notes without one, and the index of its vibrato
        /// preset, written by its Set as Default. The defaults are kit::Vibrato::utauDefault()
        /// and the first preset.
        kit::Vibrato pitchControlVibrato() const;
        void setPitchControlVibrato(const kit::Vibrato &vibrato);
        int pitchControlVibratoPreset() const;
        void setPitchControlVibratoPreset(int index);

        /// Whether the piano roll shows the pitch curves (View > Show Pitch). The default is
        /// true.
        bool isPitchVisible() const;
        void setPitchVisible(bool visible);

        /// Whether the piano roll shows the pitch that the resampler receives (View > Show
        /// Rendered Pitch). The default is true.
        bool isRenderedPitchVisible() const;
        void setRenderedPitchVisible(bool visible);

        /// Whether the piano roll shows the envelopes (View > Show Envelopes). The default is
        /// true.
        bool areEnvelopesVisible() const;
        void setEnvelopesVisible(bool visible);

        /// Whether the piano roll shows the modulation and the flags of the notes (View > Show
        /// Parameters). The default is false.
        bool areParametersVisible() const;
        void setParametersVisible(bool visible);

        /// Whether the project window shows its tool bar (View > Show Toolbar). The default is
        /// true.
        bool isToolBarVisible() const;
        void setToolBarVisible(bool visible);

        /// The language of the interface, as a locale name such as \c zh_CN, or empty for that of
        /// the system, the default. It takes effect at the next start, see Translations.
        QString language() const;
        void setLanguage(const QString &language);

        /// Encoding initially selected when a UST is exported. The default is UTF-8.
        QString ustExportCharset() const;
        void setUstExportCharset(const QString &charset);

        /// Returns the commands last chosen in the command palette, most recent first. The list
        /// is shared by all windows.
        QStringList recentCommands() const;

        /// Places \a id first among the recent commands. At most \c recentCommandCount commands
        /// are stored.
        void addRecentCommand(const QString &id);

        static constexpr int recentCommandCount = 20;

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

        /// \name Values by key
        ///
        /// Access to any value of the file by its key. A key consists of the names of the
        /// enclosing groups and the name of the value, joined by slashes, such as
        /// \c synthTools/resampler. The functions above use these functions for the values of the
        /// application.
        /// @{

        /// Returns the value at \a key, or an undefined value if absent.
        QJsonValue value(const QString &key) const;

        /// Replaces the value at \a key, creating the enclosing groups, and writes the file. An
        /// undefined or null \a value removes the value, together with each group that the
        /// removal leaves empty.
        void setValue(const QString &key, const QJsonValue &value);
        /// @}

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;

        Q_DISABLE_COPY(AppSettings)
    };

}

#endif // HELLOUTAU_EDITOR_APPSETTINGS_H
