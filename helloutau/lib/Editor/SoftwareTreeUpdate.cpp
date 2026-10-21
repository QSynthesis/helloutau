#include "SoftwareTreeUpdate.h"

#include <filesystem>
#include <system_error>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryFile>
#include <QtCore/QThread>

#ifdef Q_OS_MACOS
#  include <sys/mount.h>

#  include <string>
#  include <vector>

#  include <QtCore/QTemporaryDir>

#  include <stdcorelib/support/popen.h>
#endif

#include <hellokit/Support/ZipExtractor.h>

namespace fs = std::filesystem;

namespace hello::daw {

    namespace {

        fs::path pathOf(const QString &path) {
            return fs::path(path.toStdU16String());
        }

#ifdef Q_OS_MACOS
        std::string utf8(const QString &text) {
            const auto bytes = text.toUtf8();
            return std::string(bytes.constData(), size_t(bytes.size()));
        }

        struct ToolResult {
            bool succeeded = false;
            // The error output of the tool, or the reason why it did not run
            QString message;
        };

        // Runs the system tool \a arguments with \a input on its standard input until it exits,
        // or until \a canceled is set, which kills it.
        ToolResult runTool(std::vector<std::string> arguments, const std::string &input,
                           const std::atomic<bool> &canceled) {
            stdc::Popen process;
            process.args(std::move(arguments));
            process.standardInput(stdc::Popen::Pipe);
            process.standardOutput(stdc::Popen::DeviceNull);
            process.standardError(stdc::Popen::Pipe);
            if (!process.start()) {
                return {false, QString::fromStdString(process.errorMessage())};
            }
            // Short rounds, so that a cancellation stops the tool
            constexpr int round = 100;
            auto output = process.communicate(input, round);
            while (!output && process.errorCode() == std::errc::timed_out) {
                if (canceled) {
                    process.kill();
                    process.communicate();
                    return {false, {}};
                }
                output = process.communicate({}, round);
            }
            if (!output) {
                return {false, QString::fromStdString(process.errorMessage())};
            }
            const auto code = process.returnCode();
            return {code && *code == 0, QString::fromStdString(std::get<1>(*output)).trimmed()};
        }
#endif

    }

    SoftwareTreeUpdate::SoftwareTreeUpdate(const QString &installDirectory, QObject *parent)
        : QObject(parent), m_installDirectory(QDir::cleanPath(installDirectory)) {
    }

    SoftwareTreeUpdate::~SoftwareTreeUpdate() {
        stopExtraction();
    }

    QString SoftwareTreeUpdate::applicationInstallDirectory() {
#ifdef Q_OS_MACOS
        // The program is in Contents/MacOS of the bundle.
        return QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../.."));
#else
        return QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/.."));
#endif
    }

    bool SoftwareTreeUpdate::isIsolated(const QString &bundle) {
#ifdef Q_OS_MACOS
        // The copies of App Translocation are in a directory of this name, on a read-only
        // mount as a disk image is.
        if (bundle.contains(QStringLiteral("/AppTranslocation/"))) {
            return true;
        }
        struct statfs volume;
        return ::statfs(QFile::encodeName(bundle).constData(), &volume) == 0 &&
               (volume.f_flags & MNT_RDONLY) != 0;
#else
        Q_UNUSED(bundle);
        return false;
#endif
    }

    QString SoftwareTreeUpdate::updaterFileName() {
#ifdef Q_OS_WINDOWS
        return QStringLiteral("hello-updater.exe");
#else
        return QStringLiteral("hello-updater");
#endif
    }

    QString SoftwareTreeUpdate::installDirectory() const {
        return m_installDirectory;
    }

    QString SoftwareTreeUpdate::newDirectory() const {
        return m_installDirectory + QStringLiteral(".new");
    }

    QString SoftwareTreeUpdate::oldDirectory() const {
        return m_installDirectory + QStringLiteral(".old");
    }

    bool SoftwareTreeUpdate::canReplace() const {
        // A file stands for the directories, because creating either requires the permission
        // to write in the parent directory.
        QTemporaryFile probe(QDir::cleanPath(m_installDirectory + QStringLiteral("/..")) +
                             QStringLiteral("/.hello-update-XXXXXX"));
        return probe.open();
    }

    void SoftwareTreeUpdate::startExtraction(const QString &archive, const QString &program,
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
        startThread([this, target, source, programPath, archive, program] {
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
        });
    }

    void SoftwareTreeUpdate::startImageCopy(const QString &diskImage, const QString &program) {
        stopExtraction();
        m_canceled = false;
        m_threadError.clear();
        m_errorString.clear();
        m_extractor.reset();

        const auto target = pathOf(newDirectory());
        const auto programPath = pathOf(newDirectory() + QLatin1Char('/') + program);
        startThread([this, target, programPath, diskImage, program] {
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
            m_threadError = copyFromImage(diskImage);
            if (m_threadError.isEmpty() && !m_canceled && !fs::is_regular_file(programPath, ec)) {
                m_threadError = tr("The disk image %1 does not contain the program %2.")
                                    .arg(QDir::toNativeSeparators(diskImage),
                                         QDir::toNativeSeparators(program));
            }
            if (!m_threadError.isEmpty() || m_canceled) {
                fs::remove_all(target, ec);
            }
        });
    }

    void SoftwareTreeUpdate::cancel() {
        if (!m_thread) {
            return;
        }
        m_canceled = true;
        if (m_extractor) {
            m_extractor->cancel();
        }
    }

    bool SoftwareTreeUpdate::isRunning() const {
        return m_thread != nullptr;
    }

    QString SoftwareTreeUpdate::errorString() const {
        return m_errorString;
    }

    QStringList SoftwareTreeUpdate::updaterArguments(qint64 pid, const QString &log,
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

    QString SoftwareTreeUpdate::defaultUpdater() {
        return QCoreApplication::applicationDirPath() + QLatin1Char('/') + updaterFileName();
    }

    QString SoftwareTreeUpdate::copyUpdater(const QString &updater, const QString &directory) {
        const auto target = directory + QLatin1Char('/') + updaterFileName();
        if (!QDir().mkpath(directory)) {
            return {};
        }
        QFile::remove(target);
        // QFile::copy() keeps the permissions, the executable bit included.
        return QFile::copy(updater, target) ? target : QString();
    }

    void SoftwareTreeUpdate::removeRemnants() const {
        QDir(newDirectory()).removeRecursively();
        QDir(oldDirectory()).removeRecursively();
    }

    QString SoftwareTreeUpdate::readLog(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QString::fromUtf8(file.readAll()).trimmed();
    }

    void SoftwareTreeUpdate::startThread(std::function<void()> work) {
        m_thread.reset(QThread::create(std::move(work)));
        const int generation = ++m_generation;
        connect(m_thread.get(), &QThread::finished, this, [this, generation] {
            if (generation == m_generation) {
                finishExtraction();
            }
        });
        m_thread->start();
    }

    QString SoftwareTreeUpdate::copyFromImage(const QString &diskImage) {
#ifdef Q_OS_MACOS
        QTemporaryDir mount(QDir::tempPath() + QStringLiteral("/hello-update-XXXXXX"));
        if (!mount.isValid()) {
            return tr("A temporary folder cannot be created: %1").arg(mount.errorString());
        }
        // hdiutil reads the answer to the license agreement of the image from its standard
        // input, and cancels the mount without the answer.
        const auto attached =
            runTool({"/usr/bin/hdiutil", "attach", utf8(diskImage), "-nobrowse", "-noautoopen",
                     "-noverify", "-readonly", "-mountpoint", utf8(mount.path())},
                    "Y\n", m_canceled);
        if (!attached.succeeded) {
            return m_canceled ? QString()
                              : tr("The disk image %1 cannot be opened: %2")
                                    .arg(QDir::toNativeSeparators(diskImage), attached.message);
        }
        QString error;
        const auto bundles =
            QDir(mount.path())
                .entryList({QStringLiteral("*.app")}, QDir::Dirs | QDir::NoDotAndDotDot);
        if (bundles.size() != 1) {
            error = tr("The disk image %1 does not contain exactly one application.")
                        .arg(QDir::toNativeSeparators(diskImage));
        } else {
            const auto copied =
                runTool({"/usr/bin/ditto", utf8(mount.path() + QLatin1Char('/') + bundles.first()),
                         utf8(newDirectory())},
                        {}, m_canceled);
            if (!copied.succeeded && !m_canceled) {
                error = tr("The application cannot be copied from the disk image %1: %2")
                            .arg(QDir::toNativeSeparators(diskImage), copied.message);
            }
        }
        // Detached after a failure and a cancellation as well, which therefore do not stop it
        const std::atomic<bool> uncanceled = false;
        const auto detach = [&mount, &uncanceled](const char *option) {
            std::vector<std::string> arguments{"/usr/bin/hdiutil", "detach", utf8(mount.path()),
                                               "-quiet"};
            if (option) {
                arguments.emplace_back(option);
            }
            return runTool(std::move(arguments), {}, uncanceled).succeeded;
        };
        if (!detach(nullptr) && !detach("-force")) {
            // The removal of the folder would reach into the volume that remains mounted.
            mount.setAutoRemove(false);
        }
        return error;
#else
        Q_UNUSED(diskImage);
        return tr("Disk images can be opened only on macOS.");
#endif
    }

    void SoftwareTreeUpdate::stopExtraction() {
        if (!m_thread) {
            return;
        }
        cancel();
        m_thread->wait();
        m_thread.reset();
    }

    void SoftwareTreeUpdate::finishExtraction() {
        m_thread->wait();
        m_thread.reset();
        if (m_canceled) {
            return;
        }
        m_errorString = m_threadError;
        Q_EMIT finished();
    }

}
