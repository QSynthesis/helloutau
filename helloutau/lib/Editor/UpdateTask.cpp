#include "UpdateTask.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <hellokit/Support/ExitLauncher.h>
#include <hellokit/Update/SoftwareReleaseDownload.h>
#include <hellokit/Update/SoftwareReleaseQuery.h>

#include <helloutau/Config.h>

#include "AppSettings.h"
#include "PortableUpdate.h"
#include "UpdateChecker.h"

namespace hello::daw {

    UpdateTask::UpdateTask(AppSettings &settings, Kind kind, const QString &installDirectory,
                           QObject *parent)
        : QObject(parent), m_settings(settings), m_kind(kind),
          m_query(new kit::SoftwareReleaseQuery(this)),
          m_download(new kit::SoftwareReleaseDownload(this)),
          m_portable(new PortableUpdate(installDirectory, this)) {
        connect(m_query, &kit::SoftwareReleaseQuery::finished, this, [this] {
            const auto &release = m_query->release();
            if (!release) {
                fail(tr("Cannot check for updates: %1").arg(m_query->errorString()));
                return;
            }
            m_settings.updates().setLastCheck(QDateTime::currentDateTimeUtc());
            const auto asset =
                UpdateChecker::assetName(m_assetPrefix, release->version,
                                         QStringLiteral(HELLOUTAU_TRIPLET), m_kind == Installed);
            if (!UpdateChecker::offers(*release, m_current, asset)) {
                setState(UpToDate);
                return;
            }
            offer(*release, asset);
        });
        connect(m_download, &kit::SoftwareReleaseDownload::progressChanged, this,
                &UpdateTask::progressChanged);
        connect(m_download, &kit::SoftwareReleaseDownload::finished, this, [this] {
            const auto file = m_download->filePath();
            if (file.isEmpty()) {
                fail(tr("Cannot download the update: %1").arg(m_download->errorString()));
                return;
            }
            prepareFrom(file);
        });
        connect(m_portable, &PortableUpdate::progressChanged, this, &UpdateTask::progressChanged);
        connect(m_portable, &PortableUpdate::finished, this, [this] {
            if (!m_portable->errorString().isEmpty()) {
                // The archive is removed with the failure, as with a cancellation.
                removeDownload();
                fail(tr("Cannot extract the update: %1").arg(m_portable->errorString()));
                return;
            }
            setState(Ready);
        });
    }

    UpdateTask::~UpdateTask() = default;

    UpdateTask::Kind UpdateTask::kind() const {
        return m_kind;
    }

    UpdateTask::State UpdateTask::state() const {
        return m_state;
    }

    const std::optional<kit::SoftwareRelease> &UpdateTask::release() const {
        return m_release;
    }

    QString UpdateTask::assetName() const {
        return m_assetName;
    }

    QString UpdateTask::errorString() const {
        return m_errorString;
    }

    void UpdateTask::check(const QUrl &latestRelease, const QString &releasePage,
                           const QVersionNumber &current, const QString &assetPrefix) {
        if (isRunning()) {
            cancel();
        }
        m_current = current;
        m_assetPrefix = assetPrefix;
        m_release.reset();
        m_assetName.clear();
        if (!m_query->start(latestRelease, releasePage)) {
            fail(tr("Cannot check for updates: %1").arg(m_query->errorString()));
            return;
        }
        setState(Checking);
    }

    void UpdateTask::offer(const kit::SoftwareRelease &release, const QString &assetName) {
        m_release = release;
        m_assetName = assetName;
        m_assetFile.clear();
        setState(Available);
    }

    void UpdateTask::prepare() {
        if (!m_release || isRunning()) {
            return;
        }
        if (m_kind == Portable && !m_portable->canReplace()) {
            fail(tr("The folder that contains %1 cannot be written, so that the update cannot be "
                    "installed there. Download the new version from the page of the release.")
                     .arg(QDir::toNativeSeparators(m_portable->installDirectory())));
            return;
        }
        setState(Downloading);
        if (!m_download->start(*m_release, m_assetName, UpdateChecker::downloadDirectory())) {
            fail(tr("Cannot download the update: %1").arg(m_download->errorString()));
        }
    }

    void UpdateTask::prepareFrom(const QString &file) {
        m_assetFile = file;
        if (m_kind == Installed) {
            setState(Ready);
            return;
        }
        setState(Extracting);
        // The program, whose presence confirms that the archive is that of the application
        const auto program =
            QStringLiteral("bin/") + QFileInfo(QCoreApplication::applicationFilePath()).fileName();
        m_portable->startExtraction(file, program, m_settings.updates().extractionThreadCount());
    }

    void UpdateTask::cancel() {
        switch (m_state) {
            case Checking:
                m_query->abort();
                setState(Idle);
                break;
            case Downloading:
                m_download->abort();
                removeDownload();
                setState(Available);
                break;
            case Extracting:
                m_portable->cancel();
                removeDownload();
                setState(Available);
                break;
            case Ready:
                unschedule();
                m_portable->removeRemnants();
                removeDownload();
                setState(Available);
                break;
            default:
                break;
        }
    }

    bool UpdateTask::isRunning() const {
        return m_state == Checking || m_state == Downloading || m_state == Extracting;
    }

    bool UpdateTask::schedule(bool relaunch) {
        const auto launcher = kit::ExitLauncher::instance();
        if (m_state != Ready || !launcher) {
            return false;
        }
        const auto key = QLatin1String(launchKey);
        if (m_kind == Installed) {
            launcher->setLaunch(key, m_assetFile, UpdateChecker::installerArguments(relaunch));
        } else {
            const auto updater = PortableUpdate::copyUpdater(UpdateChecker::downloadDirectory());
            if (updater.isEmpty()) {
                return false;
            }
            launcher->setLaunch(
                key, updater,
                m_portable->updaterArguments(QCoreApplication::applicationPid(), updaterLog(),
                                             relaunch ? QCoreApplication::applicationFilePath()
                                                      : QString()));
        }
        // The installation starts the application again itself.
        if (relaunch) {
            launcher->removeLaunch(QLatin1String(kit::ExitLauncher::restartKey));
        }
        return true;
    }

    void UpdateTask::unschedule() {
        if (const auto launcher = kit::ExitLauncher::instance()) {
            launcher->removeLaunch(QLatin1String(launchKey));
        }
    }

    bool UpdateTask::isScheduled() const {
        const auto launcher = kit::ExitLauncher::instance();
        return launcher && launcher->hasLaunch(QLatin1String(launchKey));
    }

    QString UpdateTask::updaterLog() {
        return UpdateChecker::downloadDirectory() + QStringLiteral("/updater.log");
    }

    void UpdateTask::setState(State state) {
        if (state != Failed) {
            m_errorString.clear();
        }
        if (m_state == state) {
            return;
        }
        m_state = state;
        Q_EMIT stateChanged(state);
    }

    void UpdateTask::fail(const QString &message) {
        m_errorString = message;
        setState(Failed);
    }

    void UpdateTask::removeDownload() {
        if (!m_assetName.isEmpty()) {
            QFile::remove(UpdateChecker::downloadDirectory() + QLatin1Char('/') + m_assetName);
        }
        if (!m_assetFile.isEmpty()) {
            QFile::remove(m_assetFile);
        }
        m_assetFile.clear();
    }

}
