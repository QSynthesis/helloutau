#ifndef HELLOKIT_SUPPORT_FILEMANAGER_H
#define HELLOKIT_SUPPORT_FILEMANAGER_H

#include <filesystem>

#include <hellokit/Support/HelloKitSupportGlobal.h>

namespace hello::kit {

    /// The file manager of the system: File Explorer on Windows, Finder on macOS, and on Linux
    /// the file manager that implements the D-Bus interface \c org.freedesktop.FileManager1.
    class HELLOKIT_SUPPORT_EXPORT FileManager {
    public:
        /// Opens the folder that contains \a path in the file manager with \a path selected.
        ///
        /// On Linux without a file manager on D-Bus, opens the folder with \c xdg-open, which
        /// selects nothing.
        ///
        /// \return whether the file manager accepted the request, which does not wait for its
        ///         window
        static bool reveal(const std::filesystem::path &path);

    private:
        FileManager() = delete;
    };

}

#endif // HELLOKIT_SUPPORT_FILEMANAGER_H
