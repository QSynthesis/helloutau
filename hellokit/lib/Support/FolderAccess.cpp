#include "FolderAccess.h"

namespace hello::kit {

    std::optional<QString> FolderAccess::unreadableReason(const std::filesystem::path &directory) {
        std::error_code error;
        std::filesystem::directory_iterator entries(directory, error);
        if (!error || error == std::errc::no_such_file_or_directory ||
            error == std::errc::not_a_directory) {
            return std::nullopt;
        }
        return reasonOf(error);
    }

    QString FolderAccess::reasonOf(const std::error_code &error) {
#ifdef Q_OS_MACOS
        if (error == std::errc::operation_not_permitted) {
            return tr("macOS denies %1 the access. Allow the access to the folder in System "
                      "Settings, Privacy & Security, Files and Folders.")
                .arg(QCoreApplication::applicationName());
        }
#endif
        // The message of the system, in the code page of the system on Windows
        return QString::fromLocal8Bit(error.message());
    }

}
