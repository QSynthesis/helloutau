#include "InnoSetup.h"

#include <system_error>

#include <stdcorelib/platform/windows/registry.h>

namespace fs = std::filesystem;

namespace hello::kit {

    std::optional<fs::path> InnoSetup::installLocation(const QString &appId) {
        using stdc::windows::RegKey;
        // The installer writes the key in HKEY_CURRENT_USER for an installation for the current
        // user, and in HKEY_LOCAL_MACHINE for an installation for all users, in the 64-bit view
        // if the installer runs in 64-bit mode. A 64-bit process reads the same view.
        const auto keyPath =
            (QStringLiteral("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\") + appId +
             QStringLiteral("_is1"))
                .toStdWString();
        for (const auto root : {RegKey::RK_CurrentUser, RegKey::RK_LocalMachine}) {
            std::error_code error;
            const auto key = RegKey(root).open(keyPath, error);
            if (error || !key.isValid()) {
                continue;
            }
            const auto value = key.value(L"InstallLocation", error);
            if (error || !value.isString() || value.toString().empty()) {
                continue;
            }
            return fs::path(value.toString());
        }
        return std::nullopt;
    }

    bool InnoSetup::isInstalledIn(const QString &appId, const fs::path &directory) {
        const auto location = installLocation(appId);
        if (!location) {
            return false;
        }
        // The recorded directory ends with a separator, and the letter case may differ.
        std::error_code error;
        return fs::equivalent(*location, directory, error);
    }

}
