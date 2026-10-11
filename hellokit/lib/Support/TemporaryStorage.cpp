#include "TemporaryStorage.h"

#include <cassert>
#include <system_error>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QLockFile>
#include <QtCore/QTemporaryDir>

namespace fs = std::filesystem;

namespace hello::kit {

    namespace {

        constexpr char lockName[] = "session.lock";

        TemporaryStorage *s_instance = nullptr;

        QString textOf(const fs::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

        fs::path pathOf(const QString &text) {
            return fs::path(text.toStdU16String());
        }

    }

    TemporaryStorage::TemporaryStorage(const fs::path &root) : m_root(root) {
        assert(!s_instance);
        s_instance = this;

        std::error_code error;
        fs::create_directories(root, error);
        // The process ID in the name only helps a person who reads the directory.
        auto session = std::make_unique<QTemporaryDir>(
            textOf(root) + QLatin1Char('/') + QString::number(QCoreApplication::applicationPid()) +
            QStringLiteral("-XXXXXX"));
        if (!session->isValid()) {
            return;
        }
        // A time limit would declare the lock of a long session stale. Only the destruction of
        // the instance or the end of the process releases it.
        auto lock = std::make_unique<QLockFile>(session->filePath(QLatin1String(lockName)));
        lock->setStaleLockTime(0);
        if (!lock->tryLock()) {
            return;
        }
        m_session = std::move(session);
        m_lock = std::move(lock);
    }

    TemporaryStorage::~TemporaryStorage() {
        s_instance = nullptr;
    }

    TemporaryStorage *TemporaryStorage::instance() {
        return s_instance;
    }

    fs::path TemporaryStorage::location() {
        if (s_instance) {
            if (const auto session = s_instance->sessionDirectory()) {
                return *session;
            }
        }
        return pathOf(QDir::tempPath());
    }

    QString TemporaryStorage::templatePath(const QString &prefix) {
        return textOf(location()) + QLatin1Char('/') + prefix + QStringLiteral("-XXXXXX");
    }

    std::optional<fs::path> TemporaryStorage::sessionDirectory() const {
        if (!m_session) {
            return std::nullopt;
        }
        return pathOf(m_session->path());
    }

    std::vector<fs::path> TemporaryStorage::exitedSessions() const {
        const auto own = sessionDirectory();
        std::vector<fs::path> exited;
        std::error_code error;
        for (const auto &entry : fs::directory_iterator(m_root, error)) {
            if (!entry.is_directory(error) || (own && entry.path() == *own)) {
                continue;
            }
            // The lock of a running process cannot be taken. The lock of an exited process is
            // stale, and taking it removes the lock file of that process.
            QLockFile lock(textOf(entry.path() / lockName));
            lock.setStaleLockTime(0);
            if (lock.tryLock()) {
                lock.unlock();
                exited.push_back(entry.path());
            }
        }
        return exited;
    }

}
