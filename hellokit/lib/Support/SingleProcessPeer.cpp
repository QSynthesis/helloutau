#include "SingleProcessPeer.h"

#include <algorithm>

#include <QtCore/QCryptographicHash>
#include <QtCore/QDataStream>
#include <QtCore/QDeadlineTimer>
#include <QtCore/QDir>
#include <QtCore/QLockFile>
#include <QtCore/QThread>
#include <QtCore/QtDebug>
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>

#ifdef Q_OS_WINDOWS
#  include <QtCore/qt_windows.h>
#endif

#include <stdcorelib/pimpl.h>

namespace hello::kit {

    namespace {

        // The first field of a request, which also identifies the version of its format
        constexpr quint32 requestMagic = 0x48535031;

        // The byte with which the primary process confirms a request
        constexpr char receipt = 'R';

        // A request holds arguments and a directory. A connection that sends more is not a
        // request of this application, and is closed.
        constexpr qint64 maxRequestSize = 1 << 20;

        constexpr auto streamVersion = QDataStream::Qt_6_0;

        // Returns the name of the lock file and of the server. The key and the home directory of
        // the user are hashed, so that the name contains no separator and is short enough for the
        // path of a Unix domain socket, which holds about 100 bytes.
        QString nameOf(const QString &key) {
            const auto scope = (key + QLatin1Char('\n') + QDir::homePath()).toUtf8();
            const auto digest = QCryptographicHash::hash(scope, QCryptographicHash::Sha256);
            return QStringLiteral("hello-") + QString::fromLatin1(digest.toHex().left(16));
        }

        // Returns the milliseconds left until deadline, at least 0
        int remainingOf(const QDeadlineTimer &deadline) {
            return int(std::max<qint64>(0, deadline.remainingTime()));
        }

    }

    class SingleProcessPeer::Impl {
    public:
        using Decl = SingleProcessPeer;

        explicit Impl(Decl *decl) : _decl(decl) {
        }

        Decl *_decl;
        QString name;
        std::unique_ptr<QLockFile> lock;
        std::unique_ptr<QLocalServer> server;

        void listen() {
            // A crashed primary process on Unix leaves the socket file behind, and the lock
            // proves that no other process listens on it.
            QLocalServer::removeServer(name);
            server = std::make_unique<QLocalServer>();
            server->setSocketOptions(QLocalServer::UserAccessOption);
            if (!server->listen(name)) {
                qWarning().noquote()
                    << "The primary process cannot listen on" << name << server->errorString();
                return;
            }
            QObject::connect(server.get(), &QLocalServer::newConnection, _decl, [this] {
                while (const auto socket = server->nextPendingConnection()) {
                    accept(socket);
                }
            });
        }

        void accept(QLocalSocket *socket) {
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [this, socket] {
                if (socket->bytesAvailable() > maxRequestSize) {
                    socket->abort();
                    return;
                }
                QDataStream in(socket);
                in.setVersion(streamVersion);
                in.startTransaction();
                quint32 magic = 0;
                QStringList arguments;
                QString workingDirectory;
                in >> magic;
                if (magic != requestMagic) {
                    in.abortTransaction();
                    socket->abort();
                    return;
                }
                in >> arguments >> workingDirectory;
                if (!in.commitTransaction()) {
                    return;
                }
                socket->write(&receipt, 1);
                socket->flush();
                socket->disconnectFromServer();
                Q_EMIT _decl->activateRequested(arguments, workingDirectory);
            });
        }
    };

    SingleProcessPeer::SingleProcessPeer(const QString &key, QObject *parent)
        : QObject(parent), _impl(std::make_unique<Impl>(this)) {
        stdc_impl_t;
        impl.name = nameOf(key);
        auto lock = std::make_unique<QLockFile>(QDir::temp().filePath(impl.name + u".lock"));
        // The lock is taken over only if its process has ended, never because of its age.
        lock->setStaleLockTime(0);
        if (lock->tryLock(0)) {
            impl.lock = std::move(lock);
            impl.listen();
        }
    }

    SingleProcessPeer::~SingleProcessPeer() = default;

    bool SingleProcessPeer::isPrimary() const {
        stdc_impl_t;
        return impl.lock != nullptr;
    }

    bool SingleProcessPeer::sendToPrimary(const QStringList &arguments,
                                          const QString &workingDirectory, int timeout) {
        stdc_impl_t;
        if (isPrimary()) {
            return false;
        }
        const QDeadlineTimer deadline(timeout);
        QLocalSocket socket;
        // The primary process may hold the lock without listening yet, as during its start.
        for (;;) {
            socket.connectToServer(impl.name);
            if (socket.waitForConnected(remainingOf(deadline))) {
                break;
            }
            if (deadline.hasExpired()) {
                return false;
            }
            QThread::msleep(50);
        }

#ifdef Q_OS_WINDOWS
        // The process that the user started may set the foreground window, and passes the
        // right on, so that the primary process can raise its window.
        AllowSetForegroundWindow(ASFW_ANY);
#endif

        QByteArray request;
        QDataStream out(&request, QIODevice::WriteOnly);
        out.setVersion(streamVersion);
        out << requestMagic << arguments << workingDirectory;
        socket.write(request);
        if (!socket.waitForBytesWritten(remainingOf(deadline))) {
            return false;
        }
        while (socket.bytesAvailable() < 1) {
            if (!socket.waitForReadyRead(remainingOf(deadline))) {
                return false;
            }
        }
        char confirmed = 0;
        return socket.read(&confirmed, 1) == 1 && confirmed == receipt;
    }

}
