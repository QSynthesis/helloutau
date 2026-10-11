#ifndef HELLOUTAU_EDITOR_UPDATECHECKER_H
#define HELLOUTAU_EDITOR_UPDATECHECKER_H

#include <memory>
#include <optional>

#include <QtCore/QLocale>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVersionNumber>

#include <hellokit/Update/SoftwareRelease.h>

#include <helloutau/Editor/AppSettings.h>

class QWidget;

namespace hello::daw {

    /// The check for a newer release of the application on GitHub or Gitee, at start and on
    /// request. A newer release is offered to the user with a link to its page. See the
    /// automatic update in docs/Distribution.md.
    class HELLOUTAU_EDITOR_EXPORT UpdateChecker : public QObject {
        Q_OBJECT
    public:
        /// The repositories of the releases and the identifiers of the assets and of the
        /// installer, recorded in \c UpdateConfig.json in the \c share/helloutau directory beside
        /// the directory of the program.
        struct HELLOUTAU_EDITOR_EXPORT Configuration {
            /// The repositories, written as <tt>owner/name</tt>
            QString gitHubRepository;
            QString giteeRepository;

            /// The start of the names of the assets, such as \c helloutau in
            /// <tt>helloutau-0.1.6.0-x64-windows-msvc.zip</tt>
            QString assetPrefix;

            /// The \c AppId of the Windows installer, the same as in
            /// dist/installer/windows/setup.iss
            QString innoSetupAppId;

            /// Returns the path of \c UpdateConfig.json beside the program.
            static QString defaultFileName();

            /// Reads the configuration in \a fileName.
            ///
            /// \return the configuration, or \c std::nullopt with the reason in \a error if the
            ///         file cannot be read or a value is missing
            static std::optional<Configuration> read(const QString &fileName, QString *error);
        };

        /// Constructs a checker with the configuration in \a fileName and \a settings, which
        /// outlive the checker.
        explicit UpdateChecker(AppSettings &settings,
                               const QString &fileName = Configuration::defaultFileName(),
                               QObject *parent = nullptr);
        ~UpdateChecker();

        /// Returns the host of \a source if the time zone of the system is in \a territory.
        static kit::SoftwareRelease::Host hostOf(AppSettings::UpdateSource source,
                                                 QLocale::Territory territory);

        /// Returns the name of the asset with \a prefix for \a version and \a triplet: the
        /// installer if \a installer is true, and the archive otherwise.
        static QString assetName(const QString &prefix, const QVersionNumber &version,
                                 const QString &triplet, bool installer);

        /// Returns whether \a release is newer than \a current and has the asset named
        /// \a asset and the \c SHA256SUMS file.
        static bool offers(const kit::SoftwareRelease &release, const QVersionNumber &current,
                           const QString &asset);

        /// Returns the configuration, or \c std::nullopt if it cannot be read.
        const std::optional<Configuration> &configuration() const;

        /// Returns whether a check is running.
        bool isRunning() const;

        /// Checks the releases from the host of the settings, if the settings enable the check at
        /// start and the last check is a day old or older. A newer release that the user has not
        /// skipped is offered over \a parent, or over the active window if \a parent no longer
        /// exists. A failure is only logged.
        void checkAtStartup(QWidget *parent);

        /// Checks the releases from the host of \a source with a progress dialog over \a parent,
        /// and reports the result over \a parent, a skipped version included. A running check is
        /// aborted first.
        void checkNow(QWidget *parent, AppSettings::UpdateSource source);

    Q_SIGNALS:
        /// Emitted when a check ends, whether it succeeded, failed or was cancelled.
        void finished();

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_EDITOR_UPDATECHECKER_H
