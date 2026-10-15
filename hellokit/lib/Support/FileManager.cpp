#include "FileManager.h"

#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtCore/QtGlobal>

#ifdef Q_OS_WIN
#  include <QtCore/qt_windows.h>
#  include <shlobj.h>
#else
#  include <string>
#  include <vector>

#  include <stdcorelib/support/popen.h>
#endif

namespace hello::kit {

    namespace {

#ifndef Q_OS_WIN
        // The milliseconds that a helper program may take before the request counts as failed
        constexpr int helperTimeout = 10000;

        // Runs args and returns whether it exits with 0 within helperTimeout.
        bool succeeds(std::vector<std::string> args) {
            stdc::Popen process;
            process.args(std::move(args));
            process.standardOutput(stdc::Popen::DeviceNull);
            process.standardError(stdc::Popen::DeviceNull);
            if (!process.start() || !process.wait(helperTimeout)) {
                return false;
            }
            const auto code = process.returnCode();
            return code && *code == 0;
        }

        std::string utf8(const QString &text) {
            const auto bytes = text.toUtf8();
            return std::string(bytes.constData(), size_t(bytes.size()));
        }
#endif

    }

    bool FileManager::reveal(const std::filesystem::path &path) {
        std::error_code error;
        auto absolute = std::filesystem::absolute(path, error);
        if (error) {
            return false;
        }
#ifdef Q_OS_WIN
        // SHOpenFolderAndSelectItems() opens the parent folder of a single fully qualified item
        // and selects the item if no child items are given, and requires COM on the calling
        // thread. A GUI thread of Qt has initialized COM already, in which case the call returns
        // S_FALSE and is balanced all the same.
        // https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shopenfolderandselectitems
        absolute.make_preferred();
        const HRESULT initialized =
            CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        PIDLIST_ABSOLUTE item = nullptr;
        HRESULT result = SHParseDisplayName(absolute.c_str(), nullptr, &item, 0, nullptr);
        if (SUCCEEDED(result)) {
            result = SHOpenFolderAndSelectItems(item, 0, nullptr, 0);
            ILFree(item);
        }
        if (SUCCEEDED(initialized)) {
            CoUninitialize();
        }
        return SUCCEEDED(result);
#elif defined(Q_OS_MACOS)
        // open -R reveals the file in Finder instead of opening it, see open(1).
        return succeeds({"open", "-R", utf8(QString::fromStdU16String(absolute.u16string()))});
#else
        // ShowItems(as uris, s startupId) of the file manager interface,
        // https://www.freedesktop.org/wiki/Specifications/file-manager-interface/. dbus-send
        // separates the elements of an array by commas, which are therefore percent-encoded.
        // With --print-reply, dbus-send waits for the reply and fails on an error.
        auto uri = QUrl::fromLocalFile(QString::fromStdU16String(absolute.u16string()))
                       .toString(QUrl::FullyEncoded);
        uri.replace(QLatin1Char(','), QLatin1String("%2C"));
        if (succeeds({"dbus-send", "--session", "--print-reply",
                      "--dest=org.freedesktop.FileManager1", "--type=method_call",
                      "/org/freedesktop/FileManager1", "org.freedesktop.FileManager1.ShowItems",
                      "array:string:" + utf8(uri), "string:"})) {
            return true;
        }
        return succeeds(
            {"xdg-open", utf8(QString::fromStdU16String(absolute.parent_path().u16string()))});
#endif
    }

}
