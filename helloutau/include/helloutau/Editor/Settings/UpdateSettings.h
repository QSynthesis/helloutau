#ifndef HELLOUTAU_EDITOR_SETTINGS_UPDATESETTINGS_H
#define HELLOUTAU_EDITOR_SETTINGS_UPDATESETTINGS_H

#include <QtCore/QDateTime>
#include <QtCore/QString>
#include <QtCore/QUrl>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    class AppSettingsStore;

    /// The automatic update, the group \c updates of the settings file. See the automatic update
    /// in docs/Distribution.md.
    class HELLOUTAU_EDITOR_EXPORT UpdateSettings {
    public:
        ~UpdateSettings();

        /// The handling of new releases at start. A check from the Help menu always reports a new
        /// release to the user.
        enum Mode {
            /// Releases are checked only from the Help menu.
            Manual,

            /// Releases are checked at start, at most once a day, and a new release is offered
            /// to the user.
            Notified,

            /// Releases are checked at start, at most once a day, and a new release is
            /// downloaded in the background and installed when the application quits.
            Automatic,
        };

        /// The handling of new releases. The default is Notified.
        Mode mode() const;
        void setMode(Mode mode);

        /// The name of the source of the releases in the update configuration, or an empty string
        /// for the automatic selection by the time zone, the default.
        QString source() const;
        void setSource(const QString &source);

        /// The time of the last successful check of the releases, or an invalid time if none.
        QDateTime lastCheck() const;
        void setLastCheck(const QDateTime &time);

        /// The version that the user chose to skip, which the check at start does not offer, or
        /// an empty string if none.
        QString skippedVersion() const;
        void setSkippedVersion(const QString &version);

        /// Number of threads that extract the archive of an update. The default is
        /// \c defaultExtractionThreadCount, and a stored value below 1 reads as the default.
        int extractionThreadCount() const;
        void setExtractionThreadCount(int count);

        static constexpr int defaultExtractionThreadCount = 4;

        /// The version whose installer was started when the application last quit, and the page
        /// of its release, by which the next start reports whether the installation succeeded.
        /// Both are empty if no installation is pending.
        QString pendingVersion() const;
        QUrl pendingPage() const;
        void setPendingInstallation(const QString &version, const QUrl &page);
        void clearPendingInstallation();

    private:
        explicit UpdateSettings(AppSettingsStore &store);

        AppSettingsStore &m_store;

        friend class AppSettings;
        Q_DISABLE_COPY(UpdateSettings)
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGS_UPDATESETTINGS_H
