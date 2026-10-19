#include "PortableUpdate.h"

#include <filesystem>
#include <system_error>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryFile>
#include <QtCore/QThread>

#include <hellokit/Support/ZipExtractor.h>

namespace fs = std::filesystem;

namespace hello::daw {

    namespace {

        fs::path pathOf(const QString &path) {
            return fs::path(path.toStdU16String());
        }

    }

    PortableUpdate::PortableUpdate(const QString &installDirectory, QObject *parent)
        : QObject(parent), m_installDirectory(QDir::cleanPath(installDirectory)) {
    }

    PortableUpdate::~PortableUpdate() {
        stopExtraction();
    }

    QString PortableUpdate::applicationInstallDirectory() {
        return QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/.."));
    }

    QString PortableUpdate::updaterFileName() {
#ifdef Q_OS_WINDOWS
        return QStringLiteral("hello-updater.exe");
#else
        return QStringLiteral("hello-updater");
#endif
    }

    QString PortableUpdate::installDirectory() const {
        return m_installDirectory;
    }

    QString PortableUpdate::newDirectory() const {
        return m_installDirectory + QStringLiteral(".new");
    }

    QString PortableUpdate::oldDirectory() const {
        return m_installDirectory + QStringLiteral(".old");
    }

    bool PortableUpdate::canReplace() const {
        // A file stands for the directories, because creating either requires the permission
        // to write in the parent directory.
        QTemporaryFile probe(QDir::cleanPath(m_installDirectory + QStringLiteral("/..")) +
                             QStringLiteral("/.hello-update-XXXXXX"));
        return probe.open();
    }

    void PortableUpdate::startExtraction(const QString &archive, const QString &program,
                                         int threadCount) {
        stopExtraction();
        m_canceled = false;
        m_threadError.clear();
        m_errorString.clear();
        m_extractor = std::make_unique<kit::ZipExtractor>();
        m_extractor->setTopDirectoryStripped(true);
        m_extractor->setThreadCount(threadCount);
        // Called on the threads of the extraction. A queued call to a destroyed object is
        // discarded. The result also stops an extraction canceled before extract() began, which
        // discards an earlier ZipExtractor::cancel().
        m_extractor->setProgressHandler([this](qint64 written, qint64 total) {
            QMetaObject::invokeMethod(
                this,
                [this, written, total] {
                    if (m_thread && !m_canceled) {
                        Q_EMIT progressChanged(written, total);
                    }
                },
                Qt::QueuedConnection);
            return !m_canceled;
        });

        const auto target = pathOf(newDirectory());
        const auto source = pathOf(archive);
        const auto programPath = pathOf(newDirectory() + QLatin1Char('/') + program);
        m_thread.reset(QThread::create([this, target, source, programPath, archive, program] {
            std::error_code ec;
            fs::remove_all(target, ec);
            if (ec) {
                m_threadError = tr("The folder %1 cannot be removed.")
                                    .arg(QDir::toNativeSeparators(newDirectory()));
                return;
            }
            if (m_canceled) {
                return;
            }
            if (!m_extractor->extract(source, target)) {
                m_threadError = m_extractor->errorString();
            } else if (!fs::is_regular_file(programPath, ec)) {
                m_threadError =
                    tr("The archive %1 does not contain the program %2.")
                        .arg(QDir::toNativeSeparators(archive), QDir::toNativeSeparators(program));
            }
            if (!m_threadError.isEmpty() || m_canceled) {
                fs::remove_all(target, ec);
            }
        }));
        const int generation = ++m_generation;
        connect(m_thread.get(), &QThread::finished, this, [this, generation] {
            if (generation == m_generation) {
                finishExtraction();
            }
        });
        m_thread->start();
    }

    void PortableUpdate::cancel() {
        if (!m_thread) {
            return;
        }
        m_canceled = true;
        m_extractor->cancel();
    }

    bool PortableUpdate::isRunning() const {
        return m_thread != nullptr;
    }

    QString PortableUpdate::errorString() const {
        return m_errorString;
    }

    QStringList PortableUpdate::updaterArguments(qint64 pid, const QString &log,
                                                 const QString &launch) const {
        QStringList arguments{
            QStringLiteral("--pid"),         QString::number(pid),
            QStringLiteral("--target"),      QDir::toNativeSeparators(m_installDirectory),
            QStringLiteral("--source"),      QDir::toNativeSeparators(newDirectory()),
            QStringLiteral("--log"),         QDir::toNativeSeparators(log),
            QStringLiteral("--show-errors"),
        };
        if (!launch.isEmpty()) {
            arguments << QStringLiteral("--launch") << QDir::toNativeSeparators(launch);
        }
        return arguments;
    }

    QString PortableUpdate::defaultUpdater() {
        return QCoreApplication::applicationDirPath() + QLatin1Char('/') + updaterFileName();
    }

    QString PortableUpdate::copyUpdater(const QString &updater, const QString &directory) {
        const auto target = directory + QLatin1Char('/') + updaterFileName();
        if (!QDir().mkpath(directory)) {
            return {};
        }
        QFile::remove(target);
        // QFile::copy() keeps the permissions, the executable bit included.
        return QFile::copy(updater, target) ? target : QString();
    }

    void PortableUpdate::removeRemnants() const {
        QDir(newDirectory()).removeRecursively();
        QDir(oldDirectory()).removeRecursively();
    }

    QString PortableUpdate::readLog(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QString::fromUtf8(file.readAll()).trimmed();
    }

    void PortableUpdate::stopExtraction() {
        if (!m_thread) {
            return;
        }
        cancel();
        m_thread->wait();
        m_thread.reset();
    }

    void PortableUpdate::finishExtraction() {
        m_thread->wait();
        m_thread.reset();
        if (m_canceled) {
            return;
        }
        m_errorString = m_threadError;
        Q_EMIT finished();
    }

}
