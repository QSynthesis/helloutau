#include "FileIntegrity.h"

#include <atomic>
#include <map>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>

#ifdef Q_OS_WIN
#  include <QtCore/qt_windows.h>
#else
#  include <fcntl.h>
#  include <grp.h>
#  include <pwd.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

#include "PlatformFileSystem.h"

namespace fs = std::filesystem;

namespace hello::kit {

    namespace {

        using Identity = FileFingerprint::Identity;

        QString textOf(const fs::path &path) {
            return QDir::toNativeSeparators(QString::fromStdU16String(path.u16string()));
        }

        // The size of one read while the content is hashed
        constexpr qint64 readSize = 64 * 1024;

#ifdef Q_OS_WIN
        std::error_code lastError() {
            return {int(GetLastError()), std::system_category()};
        }

        // An open file, closed on destruction
        class Handle {
        public:
            Handle() = default;
            Handle(Handle &&other) noexcept : m_handle(std::exchange(other.m_handle, nullptr)) {
            }
            Handle &operator=(Handle &&other) noexcept {
                std::swap(m_handle, other.m_handle);
                return *this;
            }
            ~Handle() {
                if (m_handle) {
                    CloseHandle(m_handle);
                }
            }

            // Opens path for reading. An exclusive handle shares neither the write nor the
            // delete access, so that no process can change, rename or delete the file while it
            // is open.
            static std::optional<Handle> open(const fs::path &path, bool exclusive,
                                              std::error_code &error) {
                const DWORD share = exclusive
                                        ? FILE_SHARE_READ
                                        : FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
                const auto handle =
                    CreateFileW(path.c_str(), GENERIC_READ, share, nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
                if (handle == INVALID_HANDLE_VALUE) {
                    error = lastError();
                    return std::nullopt;
                }
                Handle result;
                result.m_handle = handle;
                return result;
            }

            std::optional<Identity> identity(std::error_code &error) const {
                BY_HANDLE_FILE_INFORMATION information;
                FILE_BASIC_INFO basic;
                if (!GetFileInformationByHandle(m_handle, &information) ||
                    !GetFileInformationByHandleEx(m_handle, FileBasicInfo, &basic, sizeof(basic))) {
                    error = lastError();
                    return std::nullopt;
                }
                if (information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    error = std::make_error_code(std::errc::is_a_directory);
                    return std::nullopt;
                }
                Identity identity;
                identity.device = information.dwVolumeSerialNumber;
                identity.index =
                    (quint64(information.nFileIndexHigh) << 32) | information.nFileIndexLow;
                identity.size =
                    qint64((quint64(information.nFileSizeHigh) << 32) | information.nFileSizeLow);
                identity.modified = basic.LastWriteTime.QuadPart;
                identity.changed = basic.ChangeTime.QuadPart;
                return identity;
            }

            // Reads up to size bytes into data, returning the number read, 0 at the end.
            std::optional<qint64> read(char *data, qint64 size, std::error_code &error) const {
                DWORD count = 0;
                if (!ReadFile(m_handle, data, DWORD(size), &count, nullptr)) {
                    error = lastError();
                    return std::nullopt;
                }
                return qint64(count);
            }

        private:
            HANDLE m_handle = nullptr;
        };

        // Returns the identity of path, opened for its attributes only, which an exclusive
        // handle of FileGuard does not deny.
        std::optional<Identity> identityAt(const fs::path &path) {
            const auto handle = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (handle == INVALID_HANDLE_VALUE) {
                return std::nullopt;
            }
            BY_HANDLE_FILE_INFORMATION information;
            FILE_BASIC_INFO basic;
            const bool read =
                GetFileInformationByHandle(handle, &information) &&
                GetFileInformationByHandleEx(handle, FileBasicInfo, &basic, sizeof(basic));
            CloseHandle(handle);
            if (!read || (information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                return std::nullopt;
            }
            Identity identity;
            identity.device = information.dwVolumeSerialNumber;
            identity.index =
                (quint64(information.nFileIndexHigh) << 32) | information.nFileIndexLow;
            identity.size =
                qint64((quint64(information.nFileSizeHigh) << 32) | information.nFileSizeLow);
            identity.modified = basic.LastWriteTime.QuadPart;
            identity.changed = basic.ChangeTime.QuadPart;
            return identity;
        }
#else
        std::error_code lastError() {
            return {errno, std::generic_category()};
        }

        Identity identityOf(const struct stat &status) {
            Identity identity;
            identity.device = quint64(status.st_dev);
            identity.index = quint64(status.st_ino);
            identity.size = qint64(status.st_size);
#  ifdef Q_OS_DARWIN
            const auto &modified = status.st_mtimespec;
            const auto &changed = status.st_ctimespec;
#  else
            const auto &modified = status.st_mtim;
            const auto &changed = status.st_ctim;
#  endif
            identity.modified = qint64(modified.tv_sec) * 1000000000 + modified.tv_nsec;
            identity.changed = qint64(changed.tv_sec) * 1000000000 + changed.tv_nsec;
            return identity;
        }

        // An open file, closed on destruction. Without a mandatory lock, an exclusive handle
        // is an ordinary one.
        class Handle {
        public:
            Handle() = default;
            Handle(Handle &&other) noexcept : m_descriptor(std::exchange(other.m_descriptor, -1)) {
            }
            Handle &operator=(Handle &&other) noexcept {
                std::swap(m_descriptor, other.m_descriptor);
                return *this;
            }
            ~Handle() {
                if (m_descriptor >= 0) {
                    ::close(m_descriptor);
                }
            }

            static std::optional<Handle> open(const fs::path &path, bool, std::error_code &error) {
                const int descriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
                if (descriptor < 0) {
                    error = lastError();
                    return std::nullopt;
                }
                Handle result;
                result.m_descriptor = descriptor;
                return result;
            }

            std::optional<Identity> identity(std::error_code &error) const {
                struct stat status;
                if (::fstat(m_descriptor, &status) != 0) {
                    error = lastError();
                    return std::nullopt;
                }
                if (!S_ISREG(status.st_mode)) {
                    error = std::make_error_code(std::errc::is_a_directory);
                    return std::nullopt;
                }
                return identityOf(status);
            }

            std::optional<qint64> read(char *data, qint64 size, std::error_code &error) const {
                for (;;) {
                    const auto count = ::read(m_descriptor, data, size_t(size));
                    if (count >= 0) {
                        return qint64(count);
                    }
                    if (errno != EINTR) {
                        error = lastError();
                        return std::nullopt;
                    }
                }
            }

        private:
            int m_descriptor = -1;
        };

        std::optional<Identity> identityAt(const fs::path &path) {
            struct stat status;
            if (::stat(path.c_str(), &status) != 0 || !S_ISREG(status.st_mode)) {
                return std::nullopt;
            }
            return identityOf(status);
        }
#endif

        // Returns the SHA-256 of the content of handle, read from its current position.
        std::optional<QString> sha256Of(const Handle &handle, std::error_code &error) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            QByteArray buffer(readSize, Qt::Uninitialized);
            for (;;) {
                const auto count = handle.read(buffer.data(), readSize, error);
                if (!count) {
                    return std::nullopt;
                }
                if (*count == 0) {
                    return QString::fromLatin1(hash.result().toHex());
                }
                hash.addData(QByteArrayView(buffer.constData(), *count));
            }
        }

        // Sets reason, if given, to the message that path cannot be read for error.
        void setUnreadable(QString *reason, const fs::path &path, const std::error_code &error) {
            if (reason) {
                *reason = FileFingerprint::tr("%1 cannot be read: %2")
                              .arg(textOf(path), PlatformFileSystem::accessErrorReason(error));
            }
        }

        std::atomic<FileFingerprintCache *> currentCache = nullptr;

    }

    std::optional<Identity> FileFingerprint::Identity::of(const fs::path &path) {
        return identityAt(path);
    }

    bool FileFingerprint::Identity::operator==(const Identity &other) const {
        return device == other.device && index == other.index && size == other.size &&
               modified == other.modified && changed == other.changed;
    }

    bool FileFingerprint::Identity::operator!=(const Identity &other) const {
        return !(*this == other);
    }

    FileFingerprint::FileFingerprint(const Identity &identity, const QString &sha256)
        : m_identity(identity), m_sha256(sha256) {
    }

    std::optional<FileFingerprint> FileFingerprint::of(const fs::path &path, QString *reason) {
        std::error_code error;
        const auto handle = Handle::open(path, false, error);
        if (!handle) {
            setUnreadable(reason, path, error);
            return std::nullopt;
        }
        const auto identity = handle->identity(error);
        if (!identity) {
            setUnreadable(reason, path, error);
            return std::nullopt;
        }
        const auto sha256 = sha256Of(*handle, error);
        if (!sha256) {
            setUnreadable(reason, path, error);
            return std::nullopt;
        }
        return FileFingerprint(*identity, *sha256);
    }

    const FileFingerprint::Identity &FileFingerprint::identity() const {
        return m_identity;
    }

    const QString &FileFingerprint::sha256() const {
        return m_sha256;
    }

    class FileFingerprintCache::Impl {
    public:
        mutable QMutex mutex;
        // By the lexically normal path
        std::map<std::u16string, FileFingerprint> fingerprints;
        std::atomic<qint64> readCount = 0;

        static std::u16string keyOf(const fs::path &path) {
            return path.lexically_normal().u16string();
        }

        // Returns the cached fingerprint of path if its identity is identity.
        std::optional<FileFingerprint> cached(const fs::path &path,
                                              const Identity &identity) const {
            const QMutexLocker locker(&mutex);
            const auto found = fingerprints.find(keyOf(path));
            if (found == fingerprints.end() || found->second.identity() != identity) {
                return std::nullopt;
            }
            return found->second;
        }

        void store(const fs::path &path, const FileFingerprint &fingerprint) {
            const QMutexLocker locker(&mutex);
            fingerprints.insert_or_assign(keyOf(path), fingerprint);
        }
    };

    FileFingerprintCache::FileFingerprintCache() : _impl(std::make_unique<Impl>()) {
        FileFingerprintCache *expected = nullptr;
        const bool first = currentCache.compare_exchange_strong(expected, this);
        Q_ASSERT(first);
        Q_UNUSED(first);
    }

    FileFingerprintCache::~FileFingerprintCache() {
        FileFingerprintCache *expected = this;
        currentCache.compare_exchange_strong(expected, nullptr);
    }

    FileFingerprintCache *FileFingerprintCache::instance() {
        return currentCache.load();
    }

    std::optional<FileFingerprint> FileFingerprintCache::fingerprintOf(const fs::path &path,
                                                                       QString *reason) {
        const auto cache = instance();
        if (!cache) {
            return FileFingerprint::of(path, reason);
        }
        auto &impl = *cache->_impl;
        if (const auto identity = Identity::of(path)) {
            if (auto found = impl.cached(path, *identity)) {
                return found;
            }
        }
        ++impl.readCount;
        auto fingerprint = FileFingerprint::of(path, reason);
        if (fingerprint) {
            impl.store(path, *fingerprint);
        }
        return fingerprint;
    }

    qint64 FileFingerprintCache::readCount() const {
        return _impl->readCount.load();
    }

    class FileGuard::Impl {
    public:
        struct Entry {
            fs::path path;
            Handle handle;
            Identity identity;
        };

        std::vector<Entry> entries;
        // The index of the first entry found changed, or -1
        mutable std::atomic<int> changed = -1;
    };

    FileGuard::FileGuard() : _impl(std::make_unique<Impl>()) {
    }

    FileGuard::FileGuard(FileGuard &&other) noexcept = default;

    FileGuard &FileGuard::operator=(FileGuard &&other) noexcept = default;

    FileGuard::~FileGuard() = default;

    std::optional<FileGuard> FileGuard::hold(const QList<File> &files, QString *reason) {
        FileGuard guard;
        const auto cache = FileFingerprintCache::instance();
        for (const auto &file : files) {
            std::error_code error;
            auto handle = Handle::open(file.path, true, error);
            if (!handle) {
                setUnreadable(reason, file.path, error);
                return std::nullopt;
            }
            const auto identity = handle->identity(error);
            if (!identity) {
                setUnreadable(reason, file.path, error);
                return std::nullopt;
            }
            // The content is read through the held handle, unless the cache knows the content
            // of this identity.
            const auto cached = cache ? cache->_impl->cached(file.path, *identity) : std::nullopt;
            std::optional<QString> sha256;
            if (cached) {
                sha256 = cached->sha256();
            } else {
                sha256 = sha256Of(*handle, error);
                if (!sha256) {
                    setUnreadable(reason, file.path, error);
                    return std::nullopt;
                }
                if (cache) {
                    ++cache->_impl->readCount;
                    cache->_impl->store(file.path, FileFingerprint(*identity, *sha256));
                }
            }
            if (sha256->compare(file.sha256, Qt::CaseInsensitive) != 0) {
                if (reason) {
                    *reason = tr("%1 has changed since it was trusted.").arg(textOf(file.path));
                }
                return std::nullopt;
            }
            guard._impl->entries.push_back({file.path, std::move(*handle), *identity});
        }
        return guard;
    }

    bool FileGuard::isIntact() const {
        const auto &impl = *_impl;
        if (impl.changed.load() >= 0) {
            return false;
        }
        for (int i = 0; i < int(impl.entries.size()); ++i) {
            const auto &entry = impl.entries[size_t(i)];
            const auto identity = Identity::of(entry.path);
            if (!identity || *identity != entry.identity) {
                int none = -1;
                impl.changed.compare_exchange_strong(none, i);
                return false;
            }
        }
        return true;
    }

    std::optional<fs::path> FileGuard::changedFile() const {
        const int index = _impl->changed.load();
        if (index < 0) {
            return std::nullopt;
        }
        return _impl->entries[size_t(index)].path;
    }

    QString FileGuard::changeMessage() const {
        const auto file = changedFile();
        return file ? tr("%1 has changed since it was trusted.").arg(textOf(*file)) : QString();
    }

    std::optional<QString> FilePermissions::writableByOthers(const fs::path &path) {
#ifdef Q_OS_WIN
        Q_UNUSED(path);
        return std::nullopt;
#else
        // A user private group, as Debian and its derivatives create one for each user. The
        // database functions are not reentrant, so that this runs on one thread.
        const auto isPrivateGroup = [](gid_t group) {
            const auto user = getpwuid(geteuid());
            if (!user || user->pw_gid != group) {
                return false;
            }
            const std::string userName = user->pw_name;
            const auto entry = getgrgid(group);
            return entry && userName == entry->gr_name && (!entry->gr_mem || !entry->gr_mem[0]);
        };
        // Returns the reason why others can write to an entry of status, named by name.
        const auto writersOf = [&](const struct stat &status,
                                   const QString &name) -> std::optional<QString> {
            if (status.st_mode & S_IWOTH) {
                return tr("Other users can write to %1.").arg(name);
            }
            if ((status.st_mode & S_IWGRP) && !isPrivateGroup(status.st_gid)) {
                return tr("The members of the group of %1 can write to it.").arg(name);
            }
            if (status.st_uid != geteuid() && status.st_uid != 0) {
                return tr("%1 belongs to another user.").arg(name);
            }
            return std::nullopt;
        };

        struct stat file;
        if (::stat(path.c_str(), &file) != 0) {
            return std::nullopt;
        }
        if (auto reason = writersOf(file, textOf(path))) {
            return reason;
        }
        const auto folder = path.parent_path();
        struct stat directory;
        if (folder.empty() || ::stat(folder.c_str(), &directory) != 0 ||
            (directory.st_mode & S_ISVTX)) {
            return std::nullopt;
        }
        return writersOf(directory, tr("the folder %1").arg(textOf(folder)));
#endif
    }

}
