#include "FileSystemWatcher.h"

#include <algorithm>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QSet>
#include <QtCore/QTimer>

namespace hello::kit {

    namespace {

        // Must match hellokit/tools/fswatcher/Protocol.h .
        constexpr char greeting[] = "hello-fswatcher 1";

        /// The maximum number of consecutive unexpected exits before monitoring is abandoned.
        /// The count resets when the program confirms its roots.
        constexpr int maxDeaths = 3;

        constexpr int restartDelay = 500;

        QByteArray escape(const QString &path) {
            QByteArray out;
            for (const char c : path.toUtf8()) {
                switch (c) {
                    case '%':
                        out += "%25";
                        break;
                    case '\n':
                        out += "%0A";
                        break;
                    case '\r':
                        out += "%0D";
                        break;
                    default:
                        out += c;
                        break;
                }
            }
            return out;
        }

        QString unescape(QByteArrayView text) {
            QByteArray out;
            for (qsizetype i = 0; i < text.size(); ++i) {
                if (text[i] == '%' && i + 2 < text.size()) {
                    const auto code = text.sliced(i + 1, 2);
                    if (code == "25" || code == "0A" || code == "0D") {
                        out += code == "25" ? '%' : code == "0A" ? '\n' : '\r';
                        i += 2;
                        continue;
                    }
                }
                out += text[i];
            }
            return QString::fromUtf8(out);
        }

        QString tidy(const QString &path) {
            return QDir::cleanPath(QDir::fromNativeSeparators(path));
        }

        /// Returns whether \a path equals \a tree or lies under it.
        bool within(const QString &path, const QString &tree) {
            return path == tree ||
                   (path.startsWith(tree) &&
                    (tree.endsWith(QLatin1Char('/')) || path.at(tree.size()) == QLatin1Char('/')));
        }

    }

    class FileSystemWatcher::Impl {
    public:
        explicit Impl(FileSystemWatcher *q) : q(q), timer(q) {
            timer.setSingleShot(true);
            QObject::connect(&timer, &QTimer::timeout, q, [this] { flush(); });
        }

        FileSystemWatcher *q;
        QString program = FileSystemWatcher::defaultProgram();
        QStringList roots;
        QTimer timer;
        int delay = 300;

        QProcess *process = nullptr;
        QByteArray pending;
        bool greeted = false;
        bool lostTrack = false;
        int deaths = 0;

        QSet<QString> directories;
        QSet<QString> trees;

        // Requested by setFileEventsEnabled(), and only if available.
        bool fileEvents = false;
        QList<FileSystemWatcher::FileEvent> events;

        void start() {
            greeted = false;
            pending.clear();
            process = new QProcess(q);
            process->setProcessChannelMode(QProcess::SeparateChannels);
            process->setStandardErrorFile(QProcess::nullDevice());

            QObject::connect(process, &QProcess::readyReadStandardOutput, q, [this] { read(); });
            QObject::connect(process, &QProcess::errorOccurred, q,
                             [this](QProcess::ProcessError error) {
                                 if (error == QProcess::FailedToStart) {
                                     giveUp();
                                 }
                             });
            QObject::connect(process, &QProcess::finished, q, [this] { died(); });

            // The roots are sent over standard input, never as arguments.
            QStringList arguments;
            if (fileEvents) {
                arguments += QStringLiteral("--file-events");
            }
            process->start(program, arguments);
        }

        void stop() {
            if (!process) {
                return;
            }
            auto *dying = process;
            process = nullptr;
            QObject::disconnect(dying, nullptr, q, nullptr);
            if (dying->state() != QProcess::NotRunning) {
                dying->write("exit\n");
                dying->closeWriteChannel();
                if (!dying->waitForFinished(2000)) {
                    dying->kill();
                    dying->waitForFinished(1000);
                }
            }
            delete dying;
        }

        void sendRoots() {
            if (!process || !greeted) {
                return;
            }
            QByteArray block = "roots\n";
            for (const auto &root : std::as_const(roots)) {
                block += escape(QDir::toNativeSeparators(root));
                block += '\n';
            }
            block += "#\n";
            process->write(block);
        }

        void died() {
            auto *dead = process;
            process = nullptr;
            if (dead) {
                dead->deleteLater();
            }
            if (++deaths > maxDeaths) {
                giveUp();
                return;
            }
            // Changes during the interruption are unknown.
            lostTrack = true;
            QTimer::singleShot(restartDelay, q, [this] {
                if (!process && !roots.isEmpty()) {
                    start();
                }
            });
        }

        void giveUp() {
            if (process) {
                auto *dead = process;
                process = nullptr;
                QObject::disconnect(dead, nullptr, q, nullptr);
                dead->kill();
                dead->deleteLater();
            }
            deaths = maxDeaths + 1;
            for (const auto &root : std::as_const(roots)) {
                Q_EMIT q->unwatchable(root);
            }
        }

        void read() {
            if (!process) {
                return;
            }
            pending += process->readAllStandardOutput();
            qsizetype end;
            while ((end = pending.indexOf('\n')) >= 0) {
                const QByteArray line = pending.left(end);
                pending.remove(0, end + 1);
                take(line);
            }
        }

        void take(const QByteArray &line) {
            if (!greeted) {
                // Any other greeting indicates a different program or an incompatible protocol.
                if (line != greeting) {
                    giveUp();
                    return;
                }
                greeted = true;
                sendRoots();
                return;
            }

            const auto space = line.indexOf(' ');
            const QByteArray word = space < 0 ? line : line.left(space);
            const QString path =
                space < 0 ? QString() : tidy(unescape(QByteArrayView(line).sliced(space + 1)));

            if (word == "dirty") {
                gather(directories, path);
            } else if (fileEvents && (word == "create" || word == "delete" || word == "change")) {
                events += FileSystemWatcher::FileEvent{
                    word == "create"   ? FileSystemWatcher::FileEvent::Created
                    : word == "delete" ? FileSystemWatcher::FileEvent::Deleted
                                       : FileSystemWatcher::FileEvent::Changed,
                    path};
                arm();
            } else if (word == "recdirty") {
                gather(trees, path);
            } else if (word == "notfound") {
                Q_EMIT q->rootNotFound(path);
            } else if (word == "unwatchable") {
                Q_EMIT q->unwatchable(path);
            } else if (word == "ok") {
                deaths = 0;
                if (lostTrack) {
                    lostTrack = false;
                    for (const auto &root : std::as_const(roots)) {
                        gather(trees, root);
                    }
                }
                Q_EMIT q->ready();
            }
        }

        void gather(QSet<QString> &into, const QString &path) {
            into.insert(path);
            arm();
        }

        void arm() {
            // A fixed interval from the first message rather than one restarted by each message,
            // because under a continuous stream of messages a restarting timer would never fire.
            if (!timer.isActive()) {
                timer.start(delay);
            }
        }

        void flush() {
            QStringList treeList(trees.begin(), trees.end());
            std::sort(treeList.begin(), treeList.end());

            QStringList directoryList;
            for (const auto &directory : std::as_const(directories)) {
                const bool covered =
                    std::any_of(treeList.begin(), treeList.end(),
                                [&](const QString &tree) { return within(directory, tree); });
                if (!covered) {
                    directoryList += directory;
                }
            }
            std::sort(directoryList.begin(), directoryList.end());

            directories.clear();
            trees.clear();
            if (!events.isEmpty()) {
                const auto reported = std::exchange(events, {});
                Q_EMIT q->fileEvents(reported);
            }
            if (!directoryList.isEmpty() || !treeList.isEmpty()) {
                Q_EMIT q->changed(directoryList, treeList);
            }
        }
    };

    FileSystemWatcher::FileSystemWatcher(QObject *parent)
        : QObject(parent), m_impl(std::make_unique<Impl>(this)) {
    }

    FileSystemWatcher::~FileSystemWatcher() {
        m_impl->stop();
    }

    bool FileSystemWatcher::fileEventsAvailable() {
#ifdef Q_OS_MACOS
        return false;
#else
        return true;
#endif
    }

    QString FileSystemWatcher::defaultProgram() {
#ifdef Q_OS_WIN
        const QString name = QStringLiteral("hello-fswatcher.exe");
#else
        const QString name = QStringLiteral("hello-fswatcher");
#endif
        return QCoreApplication::applicationDirPath() + QLatin1Char('/') + name;
    }

    void FileSystemWatcher::setProgram(const QString &program) {
        m_impl->program = program;
    }

    QString FileSystemWatcher::program() const {
        return m_impl->program;
    }

    void FileSystemWatcher::setRoots(const QStringList &roots) {
        auto &impl = *m_impl;
        QStringList tidied;
        for (const auto &root : roots) {
            tidied += tidy(QFileInfo(root).absoluteFilePath());
        }
        tidied.removeDuplicates();
        impl.roots = tidied;

        if (tidied.isEmpty()) {
            impl.stop();
            return;
        }
        // Retried after monitoring was abandoned, because the program may be available now.
        impl.deaths = 0;
        if (impl.process) {
            impl.sendRoots();
        } else {
            impl.start();
        }
    }

    QStringList FileSystemWatcher::roots() const {
        return m_impl->roots;
    }

    void FileSystemWatcher::setFileEventsEnabled(bool enabled) {
        m_impl->fileEvents = enabled && fileEventsAvailable();
    }

    bool FileSystemWatcher::fileEventsEnabled() const {
        return m_impl->fileEvents;
    }

    void FileSystemWatcher::setDelay(int milliseconds) {
        m_impl->delay = milliseconds;
    }

    int FileSystemWatcher::delay() const {
        return m_impl->delay;
    }

    qint64 FileSystemWatcher::processId() const {
        return m_impl->process ? m_impl->process->processId() : 0;
    }

}
