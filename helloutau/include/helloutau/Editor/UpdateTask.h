#ifndef HELLOUTAU_EDITOR_UPDATETASK_H
#define HELLOUTAU_EDITOR_UPDATETASK_H

#include <optional>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtCore/QVersionNumber>

#include <hellokit/Update/SoftwareRelease.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::kit {
    class SoftwareReleaseDownload;
    class SoftwareReleaseQuery;
}

namespace hello::daw {

    class AppSettings;
    class PortableUpdate;

    /// One update of the application, from the check to the scheduled installation. The task
    /// runs in the background, independent of the windows and of the update dialog, which only
    /// shows its state and calls its functions. The download, the verification and, for the
    /// portable application, the extraction form one step for the user, which cancel()
    /// undoes entirely. See the automatic update in docs/Distribution.md.
    class HELLOUTAU_EDITOR_EXPORT UpdateTask : public QObject {
        Q_OBJECT
    public:
        /// The kind of the running application, which determines the asset and the installation
        enum Kind {
            /// Installed by the installer, which replaces its files
            Installed,

            /// Extracted from an archive, whose directory hello-updater replaces
            Portable,
        };

        enum State {
            Idle,
            Checking,
            /// The latest release is not newer than the running application.
            UpToDate,
            /// A newer release is offered, see release().
            Available,
            Downloading,
            /// The archive of the portable application is extracted.
            Extracting,
            /// The installation can be scheduled.
            Ready,
            /// The last step failed, see errorString().
            Failed,
        };

        /// Constructs the task of an application of \a kind with \a settings, which outlive the
        /// task. A portable application is installed in \a installDirectory.
        UpdateTask(AppSettings &settings, Kind kind, const QString &installDirectory,
                   QObject *parent = nullptr);

        /// Cancels a running step, without the removal of its files.
        ~UpdateTask();

        Kind kind() const;
        State state() const;

        /// The release offered, or \c std::nullopt before an offer
        const std::optional<kit::SoftwareRelease> &release() const;

        /// The name of the asset of release() for kind()
        QString assetName() const;

        /// The reason of the state Failed, or an empty string otherwise
        QString errorString() const;

        /// Checks the latest release at \a latestRelease, with \a releasePage as the template of
        /// the page of a release, and offers it if it is newer than \a current and has the asset
        /// of kind() among the assets named after \a assetPrefix. A running step is canceled
        /// first.
        void check(const QUrl &latestRelease, const QString &releasePage,
                   const QVersionNumber &current, const QString &assetPrefix);

        /// Offers \a release with the asset \a assetName, as check() does once a newer release
        /// is found.
        void offer(const kit::SoftwareRelease &release, const QString &assetName);

        /// Downloads and verifies the asset of the offered release into
        /// UpdateChecker::downloadDirectory(), and extracts it for the portable application.
        /// The state is Ready once the installation can be scheduled.
        void prepare();

        /// Prepares the installation from \a file, the asset of the offered release already
        /// downloaded and verified, as prepare() does after its download.
        void prepareFrom(const QString &file);

        /// Cancels the running step and removes the files and directories it wrote: the partial or
        /// verified download and the extracted directory. The state returns to Available, or to
        /// Idle without an offer.
        void cancel();

        /// Returns whether a step is running.
        bool isRunning() const;

        /// Schedules the installation at exit with kit::ExitLauncher: the installer for the
        /// installed application, and hello-updater copied into the download directory for the
        /// portable application. If \a relaunch is true, the application starts again after the
        /// installation, and a scheduled restart is removed.
        ///
        /// \return whether the installation was scheduled, which requires the state Ready and an
        ///         instance of kit::ExitLauncher
        bool schedule(bool relaunch);

        /// The program that schedule() copies as hello-updater for the portable application,
        /// PortableUpdate::defaultUpdater() unless setUpdater() specifies another
        QString updater() const;
        void setUpdater(const QString &updater);

        /// Removes the scheduled installation.
        void unschedule();

        /// Returns whether the installation is scheduled.
        bool isScheduled() const;

        /// The key of the installation among the launches of kit::ExitLauncher
        static constexpr char launchKey[] = "update";

        /// The log of hello-updater in the download directory
        static QString updaterLog();

    Q_SIGNALS:
        void stateChanged(State state);

        /// Reports the progress of the step in progress, the bytes downloaded or written of
        /// \a total, or -1 if unknown.
        void progressChanged(qint64 done, qint64 total);

    private:
        void setState(State state);
        void fail(const QString &message);
        void removeDownload();

        AppSettings &m_settings;
        Kind m_kind;
        State m_state = Idle;
        std::optional<kit::SoftwareRelease> m_release;
        QString m_assetName;
        QString m_assetFile;
        QString m_errorString;
        QVersionNumber m_current;
        QString m_assetPrefix;
        QString m_updater;
        kit::SoftwareReleaseQuery *m_query;
        kit::SoftwareReleaseDownload *m_download;
        PortableUpdate *m_portable;
    };

}

#endif // HELLOUTAU_EDITOR_UPDATETASK_H
