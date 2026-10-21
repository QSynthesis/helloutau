#ifndef HELLOKIT_SUPPORT_FILEINTEGRITY_H
#define HELLOKIT_SUPPORT_FILEINTEGRITY_H

#include <filesystem>
#include <memory>
#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QtGlobal>

#include <hellokit/Support/HelloKitSupportGlobal.h>

namespace hello::kit {

    /// A file as it was read: its identity on the file system and the SHA-256 of its content.
    /// See docs/ProgramIntegrity.md.
    ///
    /// The identity changes whenever the file is replaced or written, so that an unchanged
    /// identity shows the content to be unchanged without reading it. A replacement by a rename
    /// yields another file index, or another \c st_ino, and a write in place changes the change
    /// time, which a program cannot set back as it can the modification time.
    class HELLOKIT_SUPPORT_EXPORT FileFingerprint {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::FileFingerprint)
    public:
        /// The identity of a file, which is only compared and never recorded. On Windows, the
        /// volume serial number, the file index, the size, the last write time and the change
        /// time. On other systems, \c st_dev, \c st_ino, the size, \c st_mtim and \c st_ctim.
        struct HELLOKIT_SUPPORT_EXPORT Identity {
            quint64 device = 0;
            quint64 index = 0;
            qint64 size = 0;
            qint64 modified = 0;
            qint64 changed = 0;

            /// Returns the identity of the file at \a path without reading its content.
            ///
            /// \return the identity, or \c std::nullopt if the file cannot be reached or is no
            ///         regular file
            static std::optional<Identity> of(const std::filesystem::path &path);

            bool operator==(const Identity &other) const;
            bool operator!=(const Identity &other) const;
        };

        /// Reads the identity and the content of the file at \a path through one handle, so that
        /// both belong to the same file.
        ///
        /// \return the fingerprint, or \c std::nullopt if the file cannot be read, with the
        ///         reason in \a reason if given
        static std::optional<FileFingerprint> of(const std::filesystem::path &path,
                                                 QString *reason = nullptr);

        const Identity &identity() const;

        /// The SHA-256 of the content in lowercase hexadecimal digits
        const QString &sha256() const;

    private:
        // FileGuard records the fingerprint that it reads through its own handle.
        friend class FileGuard;

        FileFingerprint(const Identity &identity, const QString &sha256);

        Identity m_identity;
        QString m_sha256;
    };

    /// The fingerprints of the files read so far, by path, so that a repeated check reads only
    /// the identity of a file.
    ///
    /// The application creates one instance, as it creates one \c QCoreApplication, and reaches
    /// it through fingerprintOf(). Every function is thread-safe.
    class HELLOKIT_SUPPORT_EXPORT FileFingerprintCache {
    public:
        /// Creates the cache. At most one instance exists at a time.
        FileFingerprintCache();
        ~FileFingerprintCache();

        /// Returns the instance, or \c nullptr if none exists.
        static FileFingerprintCache *instance();

        /// Returns the fingerprint of the file at \a path: the cached fingerprint if the identity
        /// of the file is unchanged, or else a fingerprint read again, which replaces it. Without
        /// an instance, the fingerprint is read each time.
        ///
        /// \return the fingerprint, or \c std::nullopt if the file cannot be read, with the
        ///         reason in \a reason if given
        static std::optional<FileFingerprint> fingerprintOf(const std::filesystem::path &path,
                                                            QString *reason = nullptr);

        /// Returns the number of times this instance has read the content of a file, for the
        /// tests.
        qint64 readCount() const;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;

        // FileGuard looks up the fingerprint of an identity that it has read through its own
        // handle.
        friend class FileGuard;

        Q_DISABLE_COPY(FileFingerprintCache)
    };

    /// Keeps a set of files as they were fingerprinted while it is held, for one render or one
    /// run of a plugin rather than for the lifetime of a window.
    ///
    /// On Windows, each file stays open without sharing the write and the delete access, so that
    /// no process can change, replace or delete it while the guard exists. Other systems have no
    /// mandatory lock, and isIntact() detects a change once it has happened. See the residual
    /// risk in docs/ProgramIntegrity.md.
    class HELLOKIT_SUPPORT_EXPORT FileGuard {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::FileGuard)
    public:
        /// A file and the SHA-256 that it is expected to have
        struct File {
            std::filesystem::path path;
            QString sha256;
        };

        FileGuard(FileGuard &&other) noexcept;
        FileGuard &operator=(FileGuard &&other) noexcept;
        ~FileGuard();

        /// Opens each of \a files and confirms its expected SHA-256. The content is read only if
        /// the identity of the open file differs from that of its fingerprint in
        /// FileFingerprintCache.
        ///
        /// \return the guard, or \c std::nullopt if a file cannot be read or has another content,
        ///         with the reason in \a reason if given
        static std::optional<FileGuard> hold(const QList<File> &files, QString *reason = nullptr);

        /// Returns whether every file still has the identity that it had when held. Once false,
        /// it stays false. Thread-safe, at the cost of reading one identity for each file.
        bool isIntact() const;

        /// Returns the first file that was found changed, or \c std::nullopt if none is.
        std::optional<std::filesystem::path> changedFile() const;

        /// Returns the message to the user about changedFile(), or an empty string if none is.
        QString changeMessage() const;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;

        FileGuard();

        Q_DISABLE_COPY(FileGuard)
    };

    /// The permissions that let users other than the current one replace a file.
    class HELLOKIT_SUPPORT_EXPORT FilePermissions {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::FilePermissions)
    public:
        /// Returns the reason why another user can replace the file at \a path, or
        /// \c std::nullopt if none can, as the \c StrictModes of ssh judges: the file or the folder
        /// that contains it is writable by the group or by others, or belongs to a user other
        /// than the current one and root. A folder with the sticky bit keeps the files of others
        /// from being renamed or deleted, so that only the file is judged in it. The group of a
        /// user private group, which has the name of the user and no other member, is the user.
        ///
        /// Always \c std::nullopt on Windows, where the access control lists are not judged.
        static std::optional<QString> writableByOthers(const std::filesystem::path &path);

    private:
        FilePermissions() = delete;
    };

}

#endif // HELLOKIT_SUPPORT_FILEINTEGRITY_H
