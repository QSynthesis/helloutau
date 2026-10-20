#ifndef HELLOKIT_SUPPORT_FOLDERACCESS_H
#define HELLOKIT_SUPPORT_FOLDERACCESS_H

#include <filesystem>
#include <optional>
#include <system_error>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>

#include <hellokit/Support/HelloKitSupportGlobal.h>

namespace hello::kit {

    /// The reasons for which a folder cannot be read, in the words that a message to the user
    /// requires. macOS denies an application the access to the Documents folder until the user
    /// allows it, and the access then fails with EPERM, for which the reason names the setting
    /// that allows the access.
    class HELLOKIT_SUPPORT_EXPORT FolderAccess {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::FolderAccess)
    public:
        /// Returns the reason for which \a directory cannot be listed, or \c std::nullopt if it
        /// can be listed or does not exist. A directory that cannot be reached at all, as in the
        /// Documents folder that macOS denies, may exist, and is reported.
        static std::optional<QString> unreadableReason(const std::filesystem::path &directory);

        /// Returns the reason that \a error of an access to a file or a folder states.
        static QString reasonOf(const std::error_code &error);

    private:
        FolderAccess() = delete;
    };

}

#endif // HELLOKIT_SUPPORT_FOLDERACCESS_H
