#ifndef HELLOUTAU_EDITOR_UPDATECHECKER_H
#define HELLOUTAU_EDITOR_UPDATECHECKER_H

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

    /// The check for a newer release of the application, at start and on request, from one of
    /// the sources of the update configuration. A newer release is offered to the user with a
    /// link to its page. See the automatic update in docs/Distribution.md.
    class HELLOUTAU_EDITOR_EXPORT UpdateChecker : public QObject {
        Q_OBJECT
    public:
        /// The sources of the releases and the identifiers of the assets and of the installer,
        /// recorded in \c UpdateConfig.json in the \c share/helloutau directory beside the
        /// directory of the program. No host is named in the code.
        struct HELLOUTAU_EDITOR_EXPORT Configuration {
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

            /// Returns the path of \c UpdateConfig.json beside the program.
            static QString defaultFileName();

            /// Reads the configuration in \a fileName.
            ///
            /// \return the configuration, or \c std::nullopt with the reason in \a error if the
            ///         file cannot be read, a value is missing, a source has no unique name or no
            ///         HTTPS address, or a territory code is unknown
            static std::optional<Configuration> read(const QString &fileName, QString *error);
        };

        /// Constructs a checker with the configuration in \a fileName and \a settings, which
        /// outlive the checker.
        explicit UpdateChecker(AppSettings &settings,
                               const QString &fileName = Configuration::defaultFileName(),
                               QObject *parent = nullptr);
        ~UpdateChecker();

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

        /// Checks the releases from the source of the settings, if the settings enable the check
        /// at start and the last check is a day old or older. A newer release that the user has
        /// not skipped is offered over \a parent, or over the active window if \a parent no
        /// longer exists. A failure is only logged.
        void checkAtStartup(QWidget *parent);

        /// Checks the releases from the source named \a source, or from the automatic selection
        /// if \a source is empty, with a progress dialog over \a parent, and reports the result
        /// over \a parent, a skipped version included. A running check is aborted first.
        void checkNow(QWidget *parent, const QString &source);

    Q_SIGNALS:
        /// Emitted when a check ends, whether it succeeded, failed or was cancelled.
        void finished();

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_EDITOR_UPDATECHECKER_H
