#include "UpdateChecker.h"

#include <algorithm>
#include <filesystem>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QPointer>
#include <QtCore/QTimeZone>
#include <QtCore/QtDebug>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QPushButton>

#include <stdcorelib/pimpl.h>

#include <hellokit/Support/SettingsFile.h>
#include <hellokit/Support/TemporaryStorage.h>
#include <hellokit/Update/SoftwareReleaseDownload.h>
#include <hellokit/Update/SoftwareReleaseQuery.h>
#ifdef Q_OS_WINDOWS
#  include <hellokit/Update/Windows/InnoSetup.h>
#endif

#include <helloutau/Config.h>

#include "UpdateDialog.h"

namespace hello::daw {

    namespace {

        constexpr char sumsName[] = "SHA256SUMS";

        // The minimum interval between two checks at start, in seconds
        constexpr qint64 startupCheckInterval = 24 * 60 * 60;

        QString textOf(const stdc::json::Object &object, const char *key) {
            const auto it = object.find(key);
            return it == object.end() ? QString() : QString::fromStdString(it->second.toString());
        }

        QVersionNumber currentVersion() {
            return QVersionNumber::fromString(QCoreApplication::applicationVersion());
        }

        // Returns whether the program runs from the directory of an installation by the Windows
        // installer. Such an installation is updated by the installer rather than by the archive.
        bool isInstalled(const UpdateChecker::Configuration &configuration) {
#ifdef Q_OS_WINDOWS
            // The program is in the bin directory of the installation directory.
            const auto directory =
                QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/.."));
            return kit::InnoSetup::isInstalledIn(configuration.innoSetupAppId,
                                                 std::filesystem::path(directory.toStdU16String()));
#else
            Q_UNUSED(configuration);
            return false;
#endif
        }

    }

    class UpdateChecker::Impl {
    public:
        using Decl = UpdateChecker;

        explicit Impl(AppSettings &settings) : settings(settings) {
        }

        UpdateChecker *decl = nullptr;
        AppSettings &settings;
        std::optional<Configuration> configuration;
        QString configurationError;
        kit::SoftwareReleaseQuery *query = nullptr;
        kit::SoftwareReleaseDownload *download = nullptr;

        // The state of the running check
        bool manual = false;
        QPointer<QWidget> parent;
        QPointer<QProgressDialog> progress;

        // The state of the running download. An interactive download is reported to the user.
        bool downloadInteractive = false;
        QPointer<QWidget> downloadParent;
        QPointer<QProgressDialog> downloadProgress;

        QWidget *dialogParent() const {
            return parent ? parent.data() : QApplication::activeWindow();
        }

        void start(QWidget *from, const QString &sourceName, bool isManual) {
            query->abort();
            closeProgress();
            manual = isManual;
            parent = from;
            if (!configuration) {
                report(tr("The update configuration cannot be read: %1").arg(configurationError));
                return;
            }
            const auto &source =
                configuration->sourceOf(sourceName, QTimeZone::systemTimeZone().territory());
            if (!query->start(source.latestRelease, source.releasePage)) {
                report(tr("Cannot check for updates: %1").arg(query->errorString()));
                return;
            }
            if (manual) {
                progress = new QProgressDialog(tr("Checking for updates..."), tr("Cancel"), 0, 0,
                                               dialogParent());
                progress->setWindowTitle(tr("Check for Updates"));
                progress->setWindowModality(Qt::WindowModal);
                progress->setAttribute(Qt::WA_DeleteOnClose);
                progress->setMinimumDuration(0);
                QObject::connect(progress, &QProgressDialog::canceled, decl, [this] {
                    query->abort();
                    closeProgress();
                    Q_EMIT decl->finished();
                });
                progress->show();
            }
        }

        void closeProgress() {
            if (progress) {
                // Disconnected first, because closing the dialog emits canceled().
                QObject::disconnect(progress, nullptr, decl, nullptr);
                progress->close();
            }
        }

        // Reports a failure to the user of a manual check, and to the log of a check at start
        void report(const QString &message) {
            if (manual) {
                QMessageBox::warning(dialogParent(), tr("Check for Updates"), message);
            } else {
                qWarning().noquote() << "The check for updates failed:" << message;
            }
            Q_EMIT decl->finished();
        }

        void readResult() {
            closeProgress();
            const auto &release = query->release();
            if (!release) {
                report(tr("Cannot check for updates: %1").arg(query->errorString()));
                return;
            }
            settings.updates().setLastCheck(QDateTime::currentDateTimeUtc());
            const auto current = currentVersion();
            const auto asset = UpdateChecker::assetName(
                configuration->assetPrefix, release->version, QStringLiteral(HELLOUTAU_TRIPLET),
                isInstalled(*configuration));
            const auto version = release->version.toString();
            if (!UpdateChecker::offers(*release, current, asset)) {
                if (manual) {
                    QMessageBox::information(
                        dialogParent(), tr("Check for Updates"),
                        tr("%1 %2 is the latest version.")
                            .arg(QApplication::applicationDisplayName(), current.toString()));
                }
                Q_EMIT decl->finished();
                return;
            }
            // Copied, because another check may replace the release of the query in the event
            // loop of the dialog
            const auto offered = *release;
            if (!manual && settings.updates().mode() == UpdateSettings::Automatic) {
                startDownload(offered, asset, false);
                Q_EMIT decl->finished();
                return;
            }
            if (!manual && version == settings.updates().skippedVersion()) {
                Q_EMIT decl->finished();
                return;
            }
            UpdateDialog dialog(version, current.toString(), offered.notes, dialogParent());
            dialog.exec();
            switch (dialog.choice()) {
                case UpdateDialog::Download:
                    startDownload(offered, asset, true);
                    break;
                case UpdateDialog::Skip:
                    settings.updates().setSkippedVersion(version);
                    break;
                case UpdateDialog::Later:
                    break;
            }
            Q_EMIT decl->finished();
        }

        void startDownload(const kit::SoftwareRelease &release, const QString &asset,
                           bool interactive) {
            closeDownloadProgress();
            downloadInteractive = interactive;
            downloadParent = dialogParent();
            if (!download->start(release, asset, UpdateChecker::downloadDirectory())) {
                reportDownload();
                return;
            }
            if (!interactive) {
                return;
            }
            downloadProgress = new QProgressDialog(tr("Downloading %1...").arg(asset), tr("Cancel"),
                                                   0, 0, downloadParent);
            downloadProgress->setWindowTitle(tr("Download Update"));
            downloadProgress->setWindowModality(Qt::WindowModal);
            downloadProgress->setAttribute(Qt::WA_DeleteOnClose);
            downloadProgress->setMinimumDuration(0);
            QObject::connect(downloadProgress, &QProgressDialog::canceled, decl, [this] {
                download->abort();
                closeDownloadProgress();
            });
            downloadProgress->show();
        }

        void closeDownloadProgress() {
            if (downloadProgress) {
                // Disconnected first, because closing the dialog emits canceled().
                QObject::disconnect(downloadProgress, nullptr, decl, nullptr);
                downloadProgress->close();
            }
        }

        void showDownloadProgress(qint64 received, qint64 total) {
            if (!downloadProgress) {
                return;
            }
            // The progress is in KiB, because the range of the dialog is an int.
            downloadProgress->setMaximum(total > 0 ? int(total / 1024) : 0);
            downloadProgress->setValue(int(received / 1024));
        }

        // Reports the result of the download to the user of an interactive download, and to the
        // log otherwise
        void reportDownload() {
            closeDownloadProgress();
            const auto filePath = download->filePath();
            if (!downloadInteractive) {
                // TODO: Install the verified update when the application quits, see the
                // installation at exit in docs/Distribution.md.
                if (filePath.isEmpty()) {
                    qWarning().noquote()
                        << "The download of the update failed:" << download->errorString();
                } else {
                    qInfo().noquote() << "The update is downloaded and verified:" << filePath;
                }
                return;
            }
            const auto parent = downloadParent.data();
            if (filePath.isEmpty()) {
                QMessageBox::warning(
                    parent, tr("Download Update"),
                    tr("Cannot download the update: %1").arg(download->errorString()));
                return;
            }
            // TODO: Install the update instead, see the installation in docs/Distribution.md.
            QMessageBox box(QMessageBox::Information, tr("Download Update"),
                            tr("The update has been downloaded and verified:\n%1")
                                .arg(QDir::toNativeSeparators(filePath)),
                            QMessageBox::Close, parent);
            const auto show = box.addButton(tr("Show in Folder"), QMessageBox::ActionRole);
            box.exec();
            if (box.clickedButton() == show) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(filePath).absolutePath()));
            }
        }
    };

    QString UpdateChecker::Configuration::defaultFileName() {
        return QDir::cleanPath(QCoreApplication::applicationDirPath() +
                               QStringLiteral("/../share/helloutau/UpdateConfig.json"));
    }

    std::optional<UpdateChecker::Configuration>
        UpdateChecker::Configuration::read(const QString &fileName, QString *error) {
        kit::DiagnosticList diagnostics;
        const auto object = kit::SettingsFile::read(fileName, diagnostics);
        if (!object) {
            *error = diagnostics.isEmpty() ? tr("The file %1 does not exist.").arg(fileName)
                                           : diagnostics.first().message;
            return std::nullopt;
        }
        Configuration configuration;
        configuration.assetPrefix = textOf(*object, "assetPrefix");
        configuration.innoSetupAppId = textOf(*object, "innoSetupAppId");
        if (configuration.assetPrefix.isEmpty() || configuration.innoSetupAppId.isEmpty()) {
            *error = tr("The file %1 lacks the prefix of the assets or the identifier of the "
                        "installer.")
                         .arg(fileName);
            return std::nullopt;
        }
        const auto sources = object->find("sources");
        if (sources != object->end()) {
            for (const auto &item : sources->second.toArray()) {
                const auto &entry = item.toObject();
                Source source;
                source.name = textOf(entry, "name");
                source.latestRelease = QUrl(textOf(entry, "latestRelease"));
                source.releasePage = textOf(entry, "releasePage");
                if (source.name.isEmpty() || configuration.hasSource(source.name)) {
                    *error = tr("A source in the file %1 has no name, or the name of another "
                                "source.")
                                 .arg(fileName);
                    return std::nullopt;
                }
                if (!source.latestRelease.isValid() ||
                    source.latestRelease.scheme() != QLatin1String("https")) {
                    *error = tr("The source %1 in the file %2 has no HTTPS address.")
                                 .arg(source.name, fileName);
                    return std::nullopt;
                }
                const auto territories = entry.find("territories");
                if (territories != entry.end()) {
                    for (const auto &code : territories->second.toArray()) {
                        const auto text = QString::fromStdString(code.toString());
                        const auto territory = QLocale::codeToTerritory(text);
                        if (territory == QLocale::AnyTerritory) {
                            *error = tr("The source %1 in the file %2 has the unknown territory "
                                        "code \"%3\".")
                                         .arg(source.name, fileName, text);
                            return std::nullopt;
                        }
                        source.territories.push_back(territory);
                    }
                }
                configuration.sources.push_back(std::move(source));
            }
        }
        if (configuration.sources.isEmpty()) {
            *error = tr("The file %1 lists no source.").arg(fileName);
            return std::nullopt;
        }
        return configuration;
    }

    const UpdateChecker::Configuration::Source &
        UpdateChecker::Configuration::sourceOf(const QString &name,
                                               QLocale::Territory territory) const {
        for (const auto &source : sources) {
            if (source.name == name) {
                return source;
            }
        }
        for (const auto &source : sources) {
            if (source.territories.contains(territory)) {
                return source;
            }
        }
        return sources.first();
    }

    bool UpdateChecker::Configuration::hasSource(const QString &name) const {
        return std::any_of(sources.begin(), sources.end(),
                           [&name](const Source &source) { return source.name == name; });
    }

    UpdateChecker::UpdateChecker(AppSettings &settings, const QString &fileName, QObject *parent)
        : QObject(parent), _impl(std::make_unique<Impl>(settings)) {
        stdc_impl_t;
        impl.decl = this;
        impl.configuration = Configuration::read(fileName, &impl.configurationError);
        impl.query = new kit::SoftwareReleaseQuery(this);
        impl.download = new kit::SoftwareReleaseDownload(this);
        connect(impl.download, &kit::SoftwareReleaseDownload::progressChanged, this,
                [this](qint64 received, qint64 total) {
                    stdc_impl_t;
                    impl.showDownloadProgress(received, total);
                });
        connect(impl.download, &kit::SoftwareReleaseDownload::finished, this, [this] {
            stdc_impl_t;
            impl.reportDownload();
        });
        connect(impl.query, &kit::SoftwareReleaseQuery::finished, this, [this] {
            stdc_impl_t;
            impl.readResult();
        });
    }

    UpdateChecker::~UpdateChecker() {
        stdc_impl_t;
        impl.closeProgress();
        impl.closeDownloadProgress();
    }

    QString UpdateChecker::downloadDirectory() {
        // The directory is in the root of the temporary storage rather than in the session
        // directory, which is removed when the application quits, before the update is installed.
        const auto storage = kit::TemporaryStorage::instance();
        const auto base = storage ? storage->root() : kit::TemporaryStorage::location();
        return QString::fromStdU16String((base / u"updates").u16string());
    }

    QString UpdateChecker::assetName(const QString &prefix, const QVersionNumber &version,
                                     const QString &triplet, bool installer) {
        return prefix + QLatin1Char('-') + version.toString() + QLatin1Char('-') + triplet +
               (installer ? QStringLiteral("-setup.exe") : QStringLiteral(".zip"));
    }

    bool UpdateChecker::offers(const kit::SoftwareRelease &release, const QVersionNumber &current,
                               const QString &asset) {
        // Normalized, so that 0.1.6 and 0.1.6.0 are the same version
        return QVersionNumber::compare(release.version.normalized(), current.normalized()) > 0 &&
               release.asset(asset) && release.asset(QLatin1String(sumsName));
    }

    const std::optional<UpdateChecker::Configuration> &UpdateChecker::configuration() const {
        stdc_impl_t;
        return impl.configuration;
    }

    bool UpdateChecker::isRunning() const {
        stdc_impl_t;
        return impl.query->isRunning();
    }

    void UpdateChecker::checkAtStartup(QWidget *parent) {
        stdc_impl_t;
        if (impl.settings.updates().mode() == UpdateSettings::Manual || impl.query->isRunning() ||
            impl.download->isRunning()) {
            return;
        }
        const auto last = impl.settings.updates().lastCheck();
        if (last.isValid() && last.secsTo(QDateTime::currentDateTimeUtc()) < startupCheckInterval) {
            return;
        }
        impl.start(parent, impl.settings.updates().source(), false);
    }

    void UpdateChecker::checkNow(QWidget *parent, const QString &source) {
        stdc_impl_t;
        impl.start(parent, source, true);
    }

}
