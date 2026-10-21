#ifndef HELLOKIT_SUPPORT_PLATFORMFILESYSTEM_H
#define HELLOKIT_SUPPORT_PLATFORMFILESYSTEM_H

#include <filesystem>
#include <optional>
#include <system_error>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>

#include <hellokit/Support/HelloKitSupportGlobal.h>

namespace hello::kit {

    /// The differences between the file systems of the platforms that the model and the messages
    /// to the user depend on: the form of listed file names, and the reasons for which a folder
    /// cannot be read.
    class HELLOKIT_SUPPORT_EXPORT PlatformFileSystem {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::PlatformFileSystem)
    public:
        /// Returns \a name, a file name of a directory listing, in the form that the model holds.
        /// On macOS the name is in Normalization Form C, as Qt reads it, because Qt writes names
        /// in Normalization Form D and the file systems of macOS find a file by either form. On
        /// other systems \a name is returned unchanged, because their file systems distinguish
        /// the two forms. See the section on the normalization of file names in docs/note.md.
        static std::filesystem::path listedFileName(const std::filesystem::path &name);

        /// Returns the reason for which \a directory cannot be listed, or \c std::nullopt if it
        /// can be listed or does not exist. A directory that cannot be reached at all, as in the
        /// Documents folder that macOS denies, may exist, and is reported.
        static std::optional<QString>
            unreadableFolderReason(const std::filesystem::path &directory);

        /// Returns the reason that \a error of an access to a file or a folder states, in the
        /// words that a message to the user requires. macOS denies an application the access to
        /// the Documents folder until the user allows it, and the access then fails with EPERM,
        /// for which the reason names the setting that allows the access.
        static QString accessErrorReason(const std::error_code &error);

    private:
        PlatformFileSystem() = delete;
    };

}

#endif // HELLOKIT_SUPPORT_PLATFORMFILESYSTEM_H
