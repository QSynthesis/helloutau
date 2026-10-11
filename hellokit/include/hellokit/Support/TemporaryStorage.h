#ifndef HELLOKIT_SUPPORT_TEMPORARYSTORAGE_H
#define HELLOKIT_SUPPORT_TEMPORARYSTORAGE_H

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include <QtCore/QString>

#include <hellokit/Support/HelloKitSupportGlobal.h>

class QLockFile;
class QTemporaryDir;

namespace hello::kit {

    /// The temporary files of the application, all in one root directory, so that removing the
    /// root directory removes every temporary file of the application. See the temporary
    /// directory manager in docs/Distribution.md.
    ///
    /// The application creates one instance, as it creates one \c QCoreApplication, and reaches
    /// it with instance(). The instance has a session directory in the root directory, locked with
    /// a \c QLockFile while the instance exists. The temporary directories and files are created
    /// in it with \c QTemporaryDir or \c QTemporaryFile from templatePath(). The destructor
    /// removes the session directory. The session directories of processes that have exited
    /// otherwise, such as crashed processes, are kept, because their files are the input of crash
    /// recovery.
    class HELLOKIT_SUPPORT_EXPORT TemporaryStorage {
    public:
        /// Creates the storage in \a root and its session directory. At most one instance exists
        /// at a time.
        explicit TemporaryStorage(const std::filesystem::path &root);

        /// Removes the session directory with its content.
        ~TemporaryStorage();

        /// Returns the instance, or \c nullptr if none exists.
        static TemporaryStorage *instance();

        /// Returns the directory in which the temporary files are created: the session directory
        /// of instance(), or else the temporary directory of the system.
        static std::filesystem::path location();

        /// Returns the template of a \c QTemporaryDir or \c QTemporaryFile in location() whose
        /// name starts with \a prefix, such as <tt>.../render-XXXXXX</tt>.
        static QString templatePath(const QString &prefix);

        inline const std::filesystem::path &root() const {
            return m_root;
        }

        /// Returns the session directory, or \c std::nullopt if it could not be created.
        std::optional<std::filesystem::path> sessionDirectory() const;

        /// Returns the session directories in root() whose processes have exited.
        std::vector<std::filesystem::path> exitedSessions() const;

    private:
        std::filesystem::path m_root;
        // The session directory, which QTemporaryDir removes with its content on destruction,
        // or null if it could not be created. Declared before the lock, so that it is destroyed
        // after the lock is released.
        std::unique_ptr<QTemporaryDir> m_session;
        // The lock in the session directory, held while the instance exists, which marks the
        // session as that of a running process for exitedSessions()
        std::unique_ptr<QLockFile> m_lock;

        Q_DISABLE_COPY(TemporaryStorage)
    };

}

#endif // HELLOKIT_SUPPORT_TEMPORARYSTORAGE_H
