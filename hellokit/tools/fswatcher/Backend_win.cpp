#include "Backend.h"

#include <cwctype>
#include <map>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>

#include <crtdbg.h>
#include <cstdlib>
#include <fcntl.h>
#include <io.h>

// Monitoring a directory without holding it open.
//
// ReadDirectoryChangesW requires a handle to the monitored directory, and an open handle
// prevents the directory from being renamed or deleted. On a voice bank, this would prevent
// its author from renaming a folder while this program runs, which a watcher must never do.
//
// Only the root directory of each drive is therefore held: one handle per drive, regardless of
// the number of monitored roots on it, reporting changes on the entire drive, with events
// outside the roots discarded here. A drive root cannot be renamed or deleted in any case. The
// JetBrains fsnotifier uses the same approach, see native/WinFsNotifier in intellij-community.
//
// Each root is resolved to its canonical file system path through a handle that is opened
// without access rights and closed immediately, because events report paths in that form:
// long names, the case stored on disk, and links resolved.

namespace fswatcher {

    namespace {

        constexpr DWORD shareAll = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

        constexpr DWORD eventMask = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                                    FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
                                    FILE_NOTIFY_CHANGE_LAST_WRITE;

        std::wstring widen(std::string_view text) {
            if (text.empty()) {
                return {};
            }
            const int size =
                MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0);
            std::wstring out(size_t(size), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), out.data(), size);
            return out;
        }

        std::string narrow(std::wstring_view text) {
            if (text.empty()) {
                return {};
            }
            const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), nullptr,
                                                 0, nullptr, nullptr);
            std::string out(size_t(size), '\0');
            WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), out.data(), size,
                                nullptr, nullptr);
            return out;
        }

        /// Returns whether two names are equal under the case-insensitive comparison of the
        /// file system.
        bool sameName(std::wstring_view a, std::wstring_view b) {
            return a.size() == b.size() && CompareStringOrdinal(a.data(), int(a.size()), b.data(),
                                                                int(b.size()), TRUE) == CSTR_EQUAL;
        }

        /// Returns whether \a path equals \a base or lies under it.
        bool within(std::wstring_view path, std::wstring_view base) {
            if (path.size() < base.size() || !sameName(path.substr(0, base.size()), base)) {
                return false;
            }
            return path.size() == base.size() || base.back() == L'\\' || path[base.size()] == L'\\';
        }

        std::wstring_view parentOf(std::wstring_view path) {
            const auto slash = path.find_last_of(L'\\');
            if (slash == std::wstring_view::npos) {
                return path;
            }
            // The parent of E:\a is E:\ , not E: , which denotes the current directory of the
            // drive.
            if (slash == 2 && path[1] == L':') {
                return path.substr(0, 3);
            }
            return path.substr(0, slash);
        }

        bool isDirectory(const std::wstring &path) {
            const DWORD attributes = GetFileAttributesW(path.c_str());
            return attributes != INVALID_FILE_ATTRIBUTES &&
                   (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        }

        /// The canonical file system path of \a path , or \c std::nullopt if it does not exist.
        std::optional<std::wstring> resolve(const std::wstring &path) {
            const HANDLE handle = CreateFileW(path.c_str(), 0, shareAll, nullptr, OPEN_EXISTING,
                                              FILE_FLAG_BACKUP_SEMANTICS, nullptr);
            if (handle == INVALID_HANDLE_VALUE) {
                return std::nullopt;
            }
            std::wstring out(MAX_PATH, L'\0');
            DWORD size = GetFinalPathNameByHandleW(handle, out.data(), DWORD(out.size()),
                                                   FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
            if (size >= out.size()) {
                out.resize(size + 1);
                size = GetFinalPathNameByHandleW(handle, out.data(), DWORD(out.size()),
                                                 FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
            }
            CloseHandle(handle);
            if (size == 0 || size >= out.size()) {
                return std::nullopt;
            }
            out.resize(size);

            if (out.rfind(L"\\\\?\\UNC\\", 0) == 0) {
                return L"\\\\" + out.substr(8);
            }
            if (out.rfind(L"\\\\?\\", 0) == 0) {
                out.erase(0, 4);
            }
            if (out.size() > 3 && out.back() == L'\\') {
                out.pop_back();
            }
            return out;
        }

        /// Returns whether a drive supports change notifications, determined as in the
        /// JetBrains implementation: a local disk with a file system known to support them. A
        /// network share is excluded, because its notifications depend on the server.
        bool reports(const std::wstring &drive) {
            const UINT type = GetDriveTypeW(drive.c_str());
            if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE && type != DRIVE_RAMDISK) {
                return false;
            }
            wchar_t system[MAX_PATH + 1] = {};
            if (!GetVolumeInformationW(drive.c_str(), nullptr, 0, nullptr, nullptr, nullptr, system,
                                       MAX_PATH + 1)) {
                return false;
            }
            for (const auto *name : {L"NTFS", L"FAT", L"FAT32", L"exFAT", L"ReFS"}) {
                if (_wcsicmp(system, name) == 0) {
                    return true;
                }
            }
            return false;
        }

    }

    class Backend::Impl {
    public:
        struct Root {
            std::string given;
            std::wstring resolved;
            std::wstring drive; ///< \c E:\ , uppercase
        };

        struct Drive {
            HANDLE stop = nullptr;
            std::thread thread;
        };

        explicit Impl(Output &out) : out(out) {
        }

        ~Impl() {
            for (auto &[name, drive] : drives) {
                halt(*drive);
            }
        }

        Output &out;

        // The roots, read by the drive threads and replaced by follow().
        std::mutex mutex;
        std::vector<Root> roots;

        // Accessed only by follow() and the destructor, both on the main thread.
        std::map<std::wstring, std::unique_ptr<Drive>> drives;

        static void halt(Drive &drive) {
            SetEvent(drive.stop);
            drive.thread.join();
            CloseHandle(drive.stop);
        }

        /// \a resolved with its root prefix restored to the form in which the root was given.
        static std::string given(const Root &root, std::wstring_view resolved) {
            auto rest = resolved.substr(root.resolved.size());
            std::string out = root.given;
            if (!rest.empty() && rest.front() != L'\\') {
                out += '\\';
            }
            out += narrow(rest);
            return out;
        }

        void report(const std::wstring &drive, const FILE_NOTIFY_INFORMATION &info) {
            std::wstring path = drive;
            path.append(info.FileName, info.FileNameLength / sizeof(wchar_t));

            const bool appeared =
                info.Action == FILE_ACTION_ADDED || info.Action == FILE_ACTION_RENAMED_NEW_NAME;
            const bool left =
                info.Action == FILE_ACTION_REMOVED || info.Action == FILE_ACTION_RENAMED_OLD_NAME;

            std::lock_guard<std::mutex> lock(mutex);
            for (const auto &root : roots) {
                if (root.drive != drive) {
                    continue;
                }

                if (within(path, root.resolved) && path.size() > root.resolved.size()) {
                    // A directory reports its own change whenever an entry in it changes, and the
                    // entry is reported as well.
                    if (info.Action == FILE_ACTION_MODIFIED && isDirectory(path)) {
                        continue;
                    }
                    out.line("dirty", given(root, parentOf(path)));
                    if (appeared && isDirectory(path)) {
                        out.line("recdirty", given(root, path));
                    }
                } else if (within(root.resolved, path)) {
                    // The root itself, or one of its ancestors.
                    if (left) {
                        out.line("gone", root.given);
                    } else if (appeared && isDirectory(root.resolved)) {
                        out.line("recdirty", root.given);
                    }
                }
            }
        }

        void everythingOn(const std::wstring &drive, const char *word) {
            std::lock_guard<std::mutex> lock(mutex);
            for (const auto &root : roots) {
                if (root.drive == drive) {
                    out.line(word, root.given);
                }
            }
        }

        void run(std::wstring drive, HANDLE stop, HANDLE armed) {
            const HANDLE directory =
                CreateFileW(drive.c_str(), FILE_LIST_DIRECTORY, shareAll, nullptr, OPEN_EXISTING,
                            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
            if (directory == INVALID_HANDLE_VALUE) {
                everythingOn(drive, "unwatchable");
                SetEvent(armed);
                return;
            }

            OVERLAPPED overlapped = {};
            overlapped.hEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);

            // DWORD-aligned, as ReadDirectoryChangesW requires.
            std::vector<DWORD> buffer(16 * 1024);
            bool first = true;

            while (true) {
                if (!ReadDirectoryChangesW(directory, buffer.data(),
                                           DWORD(buffer.size() * sizeof(DWORD)), TRUE, eventMask,
                                           nullptr, &overlapped, nullptr)) {
                    everythingOn(drive, "unwatchable");
                    break;
                }
                if (first) {
                    SetEvent(armed);
                    first = false;
                }

                const HANDLE handles[] = {stop, overlapped.hEvent};
                const DWORD woken = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
                DWORD size = 0;
                if (woken != WAIT_OBJECT_0 + 1) {
                    // The buffer is owned by the system until the request completes.
                    CancelIoEx(directory, &overlapped);
                    GetOverlappedResult(directory, &overlapped, &size, TRUE);
                    break;
                }
                if (!GetOverlappedResult(directory, &overlapped, &size, FALSE)) {
                    everythingOn(drive, "unwatchable");
                    break;
                }

                // The events exceeded the buffer, and the excess is lost, so any part of the
                // drive may have changed. A short delay lets a burst complete instead of
                // overflowing the buffer again immediately.
                if (size == 0) {
                    if (WaitForSingleObject(stop, 500) == WAIT_OBJECT_0) {
                        break;
                    }
                    everythingOn(drive, "recdirty");
                    continue;
                }

                const auto *at = reinterpret_cast<const BYTE *>(buffer.data());
                while (true) {
                    const auto &info = *reinterpret_cast<const FILE_NOTIFY_INFORMATION *>(at);
                    report(drive, info);
                    if (info.NextEntryOffset == 0) {
                        break;
                    }
                    at += info.NextEntryOffset;
                }
            }

            if (first) {
                SetEvent(armed);
            }
            CloseHandle(overlapped.hEvent);
            CloseHandle(directory);
        }
    };

    void prepareProcess() {
        // Binary mode for both streams. Text mode would add a carriage return to every line.
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);

        // No user can respond to a dialog. A dialog would block the process instead of letting
        // it terminate, and the watcher restarts the process only after it has terminated.
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
        _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
    }

    Backend::Backend(Output &out) : m_impl(std::make_unique<Impl>(out)) {
    }

    Backend::~Backend() = default;

    void Backend::follow(const std::vector<std::string> &roots) {
        auto &impl = *m_impl;

        std::vector<Impl::Root> next;
        std::vector<std::string> gone;
        std::vector<std::string> unwatchable;

        for (const auto &given : roots) {
            const auto resolved = resolve(widen(given));
            if (!resolved) {
                gone.push_back(given);
                continue;
            }
            if (resolved->size() < 3 || (*resolved)[1] != L':') {
                unwatchable.push_back(given);
                continue;
            }
            std::wstring drive = resolved->substr(0, 3);
            drive[0] = wchar_t(std::towupper(drive[0]));
            if (!reports(drive)) {
                unwatchable.push_back(given);
                continue;
            }
            std::wstring path = *resolved;
            path[0] = drive[0];
            next.push_back({given, path, drive});
        }

        {
            std::lock_guard<std::mutex> lock(impl.mutex);
            impl.roots = next;
        }

        std::map<std::wstring, bool> needed;
        for (const auto &root : next) {
            needed[root.drive] = true;
        }

        for (auto it = impl.drives.begin(); it != impl.drives.end();) {
            if (needed.count(it->first) == 0) {
                Impl::halt(*it->second);
                it = impl.drives.erase(it);
            } else {
                ++it;
            }
        }

        for (const auto &[name, yes] : needed) {
            if (impl.drives.count(name) != 0) {
                continue;
            }
            auto drive = std::make_unique<Impl::Drive>();
            drive->stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            const HANDLE armed = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            drive->thread = std::thread(&Impl::run, &impl, name, drive->stop, armed);
            WaitForSingleObject(armed, 10000);
            CloseHandle(armed);
            impl.drives[name] = std::move(drive);
        }

        for (const auto &root : gone) {
            impl.out.line("gone", root);
        }
        for (const auto &root : unwatchable) {
            impl.out.line("unwatchable", root);
        }
        impl.out.line("ok");
    }

}
