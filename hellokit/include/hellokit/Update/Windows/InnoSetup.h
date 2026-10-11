#ifndef HELLOKIT_UPDATE_WINDOWS_INNOSETUP_H
#define HELLOKIT_UPDATE_WINDOWS_INNOSETUP_H

#include <filesystem>
#include <optional>

#include <QtCore/QString>

#include <hellokit/Update/HelloKitUpdateGlobal.h>

namespace hello::kit {

    /// The installation records of Inno Setup, by which an installed application is
    /// distinguished from an extracted one. Available only on Windows.
    class HELLOKIT_UPDATE_EXPORT InnoSetup {
    public:
        /// Returns the installation directory that the installer with the \c AppId \a appId
        /// recorded in its uninstall key, <tt>Software\\Microsoft\\Windows\\CurrentVersion\\
        /// Uninstall\\<appId>_is1</tt>, in \c HKEY_CURRENT_USER and then in
        /// \c HKEY_LOCAL_MACHINE.
        ///
        /// \return the directory, or \c std::nullopt if no such key or value exists
        static std::optional<std::filesystem::path> installLocation(const QString &appId);

        /// Returns whether \a directory is the installation directory of \a appId, see
        /// installLocation().
        static bool isInstalledIn(const QString &appId, const std::filesystem::path &directory);

        /// Creates the mutex named \a name, for which the installer of an update waits, and holds
        /// it until the process ends. The handle is never closed, so that the mutex disappears
        /// only when the process has ended and its files are no longer in use.
        ///
        /// \return whether the mutex was created, or opened if it exists
        static bool holdMutex(const QString &name);
    };

}

#endif // HELLOKIT_UPDATE_WINDOWS_INNOSETUP_H
