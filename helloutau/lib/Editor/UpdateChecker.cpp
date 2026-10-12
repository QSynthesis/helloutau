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
#include <QtWidgets/QPushButton>

#include <stdcorelib/pimpl.h>

#include <hellokit/Support/SettingsFile.h>
#include <hellokit/Support/TemporaryStorage.h>
#ifdef Q_OS_WINDOWS
#  include <hellokit/Update/Windows/InnoSetup.h>
#endif

#include <helloutau/Config.h>

#include "PortableUpdate.h"
#include "UpdateDialog.h"
#include "UpdateTask.h"
#include "UpdateTaskView.h"

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
        bool isInstalled(const UpdateChecker::ManifestFile &manifest) {
#ifdef Q_OS_WINDOWS
            // The program is in the bin directory of the installation directory.
            const auto directory =
                QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/.."));
            return kit::InnoSetup::isInstalledIn(manifest.innoSetupAppId,
                                                 std::filesystem::path(directory.toStdU16String()));
#else
            Q_UNUSED(manifest);
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
        std::optional<ManifestFile> manifest;
        QString manifestError;
        UpdateTask *task = nullptr;
        std::function<bool()> closeWindows;

        // Whether the running task was started by the check at start, whose result is acted on
        // without a request of the user
        bool atStartup = false;
        // Whether a check runs, for finished()
        bool checking = false;
        // The source of the last check, which Check Again uses
        QString source;
        QPointer<QWidget> parent;
        QPointer<UpdateDialog> dialog;

        QWidget *dialogParent() const {
            return parent ? parent.data() : QApplication::activeWindow();
        }

        void check() {
            const auto &found =
                manifest->sourceOf(source, QTimeZone::systemTimeZone().territory());
            task->check(found.latestRelease, found.releasePage, currentVersion(),
                        manifest->assetPrefix);
        }

        // Shows the dialog of the task, which is modal, unless it is open already
        void showDialog() {
            if (dialog) {
                dialog->raise();
                return;
            }
            UpdateDialog box(task, settings, dialogParent());
            box.view()->setCloseWindows(closeWindows);
            box.view()->setCheck([this] {
                atStartup = false;
                check();
            });
            dialog = &box;
            box.exec();
        }

        // Acts on the task that the check at start began. In the mode Notified a release that
        // the user has not skipped is offered in the dialog. In the mode Automatic the release
        // is prepared and its installation scheduled at exit.
        void followStartup(UpdateTask::State state) {
            if (!atStartup) {
                return;
            }
            switch (state) {
                case UpdateTask::Available:
                    if (settings.updates().mode() == UpdateSettings::Automatic) {
                        task->prepare();
                        return;
                    }
                    atStartup = false;
                    if (task->release()->version.toString() !=
                        settings.updates().skippedVersion()) {
                        showDialog();
                    }
                    return;
                case UpdateTask::Ready:
                    atStartup = false;
                    task->schedule(false);
                    return;
                case UpdateTask::Failed:
                    atStartup = false;
                    qWarning().noquote() << "The automatic update failed:" << task->errorString();
                    return;
                case UpdateTask::Idle:
                case UpdateTask::UpToDate:
                    atStartup = false;
                    return;
                default:
                    return;
            }
        }
    };

    QString UpdateChecker::ManifestFile::defaultFileName() {
        return QDir::cleanPath(QCoreApplication::applicationDirPath() +
                               QStringLiteral("/../share/helloutau/UpdateManifest.json"));
    }

    std::optional<UpdateChecker::ManifestFile>
        UpdateChecker::ManifestFile::read(const QString &fileName, QString *error) {
        kit::DiagnosticList diagnostics;
        const auto object = kit::SettingsFile::read(fileName, diagnostics);
        if (!object) {
            *error = diagnostics.isEmpty() ? tr("The file %1 does not exist.").arg(fileName)
                                           : diagnostics.first().message;
            return std::nullopt;
        }
        ManifestFile manifest;
        manifest.assetPrefix = textOf(*object, "assetPrefix");
        manifest.innoSetupAppId = textOf(*object, "innoSetupAppId");
        if (manifest.assetPrefix.isEmpty() || manifest.innoSetupAppId.isEmpty()) {
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
                if (source.name.isEmpty() || manifest.hasSource(source.name)) {
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
                manifest.sources.push_back(std::move(source));
            }
        }
        if (manifest.sources.isEmpty()) {
            *error = tr("The file %1 lists no source.").arg(fileName);
            return std::nullopt;
        }
        return manifest;
    }

    const UpdateChecker::ManifestFile::Source &
        UpdateChecker::ManifestFile::sourceOf(const QString &name,
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

    bool UpdateChecker::ManifestFile::hasSource(const QString &name) const {
        return std::any_of(sources.begin(), sources.end(),
                           [&name](const Source &source) { return source.name == name; });
    }

    UpdateChecker::UpdateChecker(AppSettings &settings, const QString &fileName, QObject *parent)
        : QObject(parent), _impl(std::make_unique<Impl>(settings)) {
        stdc_impl_t;
        impl.decl = this;
        impl.manifest = ManifestFile::read(fileName, &impl.manifestError);
        const auto kind = impl.manifest && isInstalled(*impl.manifest)
                              ? UpdateTask::Installed
                              : UpdateTask::Portable;
        impl.task =
            new UpdateTask(settings, kind, PortableUpdate::applicationInstallDirectory(), this);
        connect(impl.task, &UpdateTask::stateChanged, this, [this](UpdateTask::State state) {
            stdc_impl_t;
            if (impl.checking && state != UpdateTask::Checking) {
                impl.checking = false;
                Q_EMIT finished();
            }
            impl.checking = state == UpdateTask::Checking;
            impl.followStartup(state);
        });
    }

    UpdateChecker::~UpdateChecker() {
        stdc_impl_t;
        // Recorded only if the installation is still scheduled, as it is when the application
        // quits normally.
        if (impl.task->isScheduled() && impl.task->release()) {
            impl.settings.updates().setPendingInstallation(impl.task->release()->version.toString(),
                                                           impl.task->release()->page);
        }
    }

    QString UpdateChecker::downloadDirectory() {
        // The directory is in the root of the temporary storage rather than in the session
        // directory, which is removed when the application quits, before the update is installed.
        const auto storage = kit::TemporaryStorage::instance();
        const auto base = storage ? storage->root() : kit::TemporaryStorage::location();
        return QString::fromStdU16String((base / u"updates").u16string());
    }

    QStringList UpdateChecker::installerArguments(bool relaunch) {
        QStringList arguments{relaunch ? QStringLiteral("/SILENT") : QStringLiteral("/VERYSILENT"),
                              QStringLiteral("/SUPPRESSMSGBOXES"), QStringLiteral("/NORESTART"),
                              QStringLiteral("/NOCLOSEAPPLICATIONS"), QStringLiteral("/UPDATE")};
        if (relaunch) {
            arguments.push_back(QStringLiteral("/RELAUNCH"));
        }
        return arguments;
    }

    void UpdateChecker::setCloseWindows(const std::function<bool()> &closeWindows) {
        stdc_impl_t;
        impl.closeWindows = closeWindows;
    }

    void UpdateChecker::holdInstallerMutex() {
#ifdef Q_OS_WINDOWS
        stdc_impl_t;
        if (impl.manifest) {
            kit::InnoSetup::holdMutex(impl.manifest->innoSetupAppId);
        }
#endif
    }

    void UpdateChecker::reportLastInstallation(QWidget *parent) {
        stdc_impl_t;
        const auto pending = impl.settings.updates().pendingVersion();
        if (pending.isEmpty()) {
            return;
        }
        const auto page = impl.settings.updates().pendingPage();
        impl.settings.updates().clearPendingInstallation();
        // Read before the removal of the download directory, which holds it
        const auto log = PortableUpdate::readLog(UpdateTask::updaterLog());
        // Removed regardless of the result, so that no download remains in the temporary directory.
        // A failed update is downloaded again.
        QDir(downloadDirectory()).removeRecursively();
        if (impl.task->kind() == UpdateTask::Portable) {
            PortableUpdate(PortableUpdate::applicationInstallDirectory()).removeRemnants();
        }
        const auto target = QVersionNumber::fromString(pending);
        if (QVersionNumber::compare(currentVersion().normalized(), target.normalized()) >= 0) {
            return;
        }
        QMessageBox box(QMessageBox::Warning, tr("Install Update"),
                        tr("The update to %1 %2 was not installed.")
                            .arg(QApplication::applicationDisplayName(), pending),
                        QMessageBox::Close, parent);
        if (!log.isEmpty()) {
            box.setDetailedText(log);
        }
        const auto open = box.addButton(tr("Open Download Page"), QMessageBox::ActionRole);
        box.exec();
        if (box.clickedButton() == open && page.isValid()) {
            QDesktopServices::openUrl(page);
        }
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

    const std::optional<UpdateChecker::ManifestFile> &UpdateChecker::manifest() const {
        stdc_impl_t;
        return impl.manifest;
    }

    bool UpdateChecker::isRunning() const {
        stdc_impl_t;
        return impl.task->isRunning();
    }

    UpdateTask *UpdateChecker::task() const {
        stdc_impl_t;
        return impl.task;
    }

    void UpdateChecker::checkAtStartup(QWidget *parent) {
        stdc_impl_t;
        if (!impl.manifest || impl.settings.updates().mode() == UpdateSettings::Manual ||
            impl.task->isRunning() || impl.task->state() != UpdateTask::Idle) {
            return;
        }
        const auto last = impl.settings.updates().lastCheck();
        if (last.isValid() && last.secsTo(QDateTime::currentDateTimeUtc()) < startupCheckInterval) {
            return;
        }
        impl.parent = parent;
        impl.source = impl.settings.updates().source();
        impl.atStartup = true;
        impl.check();
    }

    void UpdateChecker::checkNow(QWidget *parent, const QString &source) {
        stdc_impl_t;
        impl.parent = parent;
        if (!impl.manifest) {
            QMessageBox::warning(
                impl.dialogParent(), tr("Check for Updates"),
                tr("The update manifest cannot be read: %1").arg(impl.manifestError));
            return;
        }
        impl.source = source;
        // A task that has a result to show is shown as it is. Otherwise the releases are
        // checked again.
        const auto state = impl.task->state();
        if (state == UpdateTask::Idle || state == UpdateTask::UpToDate ||
            (state == UpdateTask::Failed && !impl.task->release())) {
            impl.atStartup = false;
            impl.check();
        }
        impl.showDialog();
    }

}
