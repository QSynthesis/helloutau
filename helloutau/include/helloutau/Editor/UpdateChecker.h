#ifndef HELLOUTAU_EDITOR_UPDATECHECKER_H
#define HELLOUTAU_EDITOR_UPDATECHECKER_H

#include <functional>
#include <memory>
#include <optional>

#include <QtCore/QList>
#include <QtCore/QLocale>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtCore/QVersionNumber>

#include <hellokit/Update/SoftwareRelease.h>

#include <helloutau/Editor/AppSettings.h>

class QWidget;

namespace hello::daw {

    class UpdateTask;

    /// The update of the application, at start and on request, from one of the sources of the
    /// update manifest. The checker holds the UpdateTask of the application, which runs in
    /// the background, and shows it in the modal update dialog. See the automatic update in
    /// docs/Distribution.md.
    class HELLOUTAU_EDITOR_EXPORT UpdateChecker : public QObject {
        Q_OBJECT
    public:
        /// The sources of the releases and the identifiers of the assets and of the installer,
        /// recorded in \c UpdateManifest.json in the \c share/helloutau/update directory beside
        /// the directory of the program. No host is named in the code.
        struct HELLOUTAU_EDITOR_EXPORT ManifestFile {
            /// A host of the releases, which the settings select by its name.
            struct Source {
                QString name;

                /// The address of the latest release in the release interface of the host, an
                /// HTTPS URL
                QUrl latestRelease;

                /// The template of the page of a release, with <tt>${tag}</tt> for the tag, for
                /// a host whose releases have no \c html_url, or an empty string
                QString releasePage;

                /// The territories for which automatic selection chooses this source
                QList<QLocale::Territory> territories;
            };

            /// The sources in the order of the settings, at least one
            QList<Source> sources;

            /// The start of the names of the assets, such as \c helloutau in
            /// <tt>helloutau-0.1.6.0-x64-windows-msvc.zip</tt>
            QString assetPrefix;

            /// The \c AppId of the Windows installer, the same as in
            /// dist/installer/windows/setup.iss
            QString innoSetupAppId;

            /// Returns the source named \a name. If \a name is empty or names no source, returns
            /// the automatic selection for \a territory: the first source whose territories
            /// include \a territory, or else the first source.
            const Source &sourceOf(const QString &name, QLocale::Territory territory) const;

            /// Returns whether a source is named \a name.
            bool hasSource(const QString &name) const;

            /// Returns the path of \c UpdateManifest.json beside the program.
            static QString defaultFileName();

            /// Reads the manifest in \a fileName.
            ///
            /// \return the manifest, or \c std::nullopt with the reason in \a error if the
            ///         file cannot be read, a value is missing, a source has no unique name or no
            ///         HTTPS address, or a territory code is unknown
            static std::optional<ManifestFile> read(const QString &fileName, QString *error);
        };

        /// Constructs a checker with the manifest in \a fileName and \a settings, which
        /// outlive the checker.
        explicit UpdateChecker(AppSettings &settings,
                               const QString &fileName = ManifestFile::defaultFileName(),
                               QObject *parent = nullptr);
        ~UpdateChecker();

        /// Returns the directory of the downloaded updates, \c updates in the root of
        /// kit::TemporaryStorage, which outlives the session so that an update is installed after
        /// the application quits. Without a storage, the directory is in the temporary
        /// directory of the system.
        static QString downloadDirectory();

        /// Returns the name of the asset with \a prefix for \a version and \a triplet: the
        /// installer if \a installer is true, and the archive otherwise.
        static QString assetName(const QString &prefix, const QVersionNumber &version,
                                 const QString &triplet, bool installer);

        /// Returns the arguments of the installer of an update: \c /SILENT with \c /RELAUNCH if
        /// \a relaunch is true, so that the installer shows its progress and starts the
        /// application again, and \c /VERYSILENT otherwise. See dist/installer/windows/setup.iss.
        static QStringList installerArguments(bool relaunch);

        /// Returns whether \a release is newer than \a current and has the asset named
        /// \a asset and the \c SHA256SUMS file.
        static bool offers(const kit::SoftwareRelease &release, const QVersionNumber &current,
                           const QString &asset);

        /// Returns the manifest, or \c std::nullopt if it cannot be read.
        const std::optional<ManifestFile> &manifest() const;

        /// Returns whether a check is running.
        bool isRunning() const;

        /// Returns the update task of the application, which the checker holds.
        UpdateTask *task() const;

        /// Sets the function that closes every window as quitting does, asking the user to save,
        /// and returns whether every window was closed. Install Now calls it before the
        /// application quits for the installation.
        void setCloseWindows(const std::function<bool()> &closeWindows);

        /// Holds the mutex for which the installer of an update waits, named after the \c AppId
        /// of the installer, until the process ends. Only on Windows, and only if the update
        /// manifest can be read.
        void holdInstallerMutex();

        /// Reports the result of the installation started when the application last quit, if
        /// any: a failure over \a parent with the page of the release. The downloaded updates
        /// are removed regardless of the result.
        void reportLastInstallation(QWidget *parent);

        /// Checks the releases from the source of the settings, unless the update mode of the
        /// settings is UpdateSettings::Manual or the last check is less than a day old. In
        /// the mode UpdateSettings::Notified, a newer release that the user has not skipped
        /// is offered in the update dialog over \a parent, or over the active window if
        /// \a parent no longer exists. In the mode UpdateSettings::Automatic, a newer release
        /// is prepared and its installation scheduled at exit without a question. A failure is
        /// only logged.
        void checkAtStartup(QWidget *parent);

        /// Shows the update dialog over \a parent. If the task has no result to show, the
        /// releases are checked first from the source named \a source, or from the automatic
        /// selection if \a source is empty. A skipped version is offered as well.
        void checkNow(QWidget *parent, const QString &source);

    Q_SIGNALS:
        /// Emitted when a check ends, whether it succeeded, failed or was canceled.
        void finished();

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_EDITOR_UPDATECHECKER_H
