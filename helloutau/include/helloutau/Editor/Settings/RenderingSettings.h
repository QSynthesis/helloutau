#ifndef HELLOUTAU_EDITOR_SETTINGS_RENDERINGSETTINGS_H
#define HELLOUTAU_EDITOR_SETTINGS_RENDERINGSETTINGS_H

#include <QtCore/QString>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    class AppSettingsStore;

    /// The synth tools, the render log and the playback, the groups \c synthTools, \c renderLog
    /// and \c playback of the settings file.
    class HELLOUTAU_EDITOR_EXPORT RenderingSettings {
    public:
        ~RenderingSettings();

        /// Synth tools used for rendering and written to an exported UST that specifies no synth
        /// tool. The synth tools specified by a project are never used without confirmation by the
        /// user, see CLAUDE.md.
        QString resampler() const;
        void setResampler(const QString &path);
        QString wavtool() const;
        void setWavtool(const QString &path);

        /// Whether the render log keeps all captured synth tool output, or only the latest run.
        bool isLogAccumulated() const;
        void setLogAccumulated(bool accumulated);

        /// Maximum size of the captured render log in bytes. The default is 1 MiB.
        int logLimit() const;
        void setLogLimit(int bytes);

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
        int threadCount() const;
        void setThreadCount(int count);

    private:
        explicit RenderingSettings(AppSettingsStore &store);

        AppSettingsStore &m_store;

        friend class AppSettings;
        Q_DISABLE_COPY(RenderingSettings)
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGS_RENDERINGSETTINGS_H
