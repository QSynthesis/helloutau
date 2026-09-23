#include "Backend.h"

#include <climits>
#include <cstdlib>
#include <mutex>

#include <CoreServices/CoreServices.h>
#include <sys/mount.h>
#include <sys/stat.h>

// Monitoring a tree with FSEvents, which monitors entire trees natively.
//
// One stream covers all roots and reports individual files. FSEvents holds no descriptor of the
// monitored directories, so nothing prevents renaming or deletion. The stream also watches the
// roots themselves, which reports a root renamed or removed, directly or through an ancestor.
//
// FSEvents reports canonical paths: /private/var for /var, with links resolved. Every root is
// resolved to that form first, and reported paths are converted back to the form in which the
// root was given.
//
// A root on a non-local volume is unwatchable, because FSEvents reports only changes made by
// this machine, not by others.

namespace fswatcher {

    namespace {

        bool isDirectory(const std::string &path) {
            struct stat info{};
            return lstat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
        }

        bool isLocal(const std::string &path) {
            struct statfs info{};
            return statfs(path.c_str(), &info) == 0 && (info.f_flags & MNT_LOCAL) != 0;
        }

        std::string parentOf(const std::string &path) {
            const auto slash = path.find_last_of('/');
            if (slash == std::string::npos || slash == 0) {
                return "/";
            }
            return path.substr(0, slash);
        }

        /// Returns whether \a path equals \a base or lies under it.
        bool within(const std::string &path, const std::string &base) {
            if (path.compare(0, base.size(), base) != 0) {
                return false;
            }
            return path.size() == base.size() || base == "/" || path[base.size()] == '/';
        }

    }

    class Backend::Impl {
    public:
        struct Root {
            std::string given;
            std::string resolved;
        };

        explicit Impl(Output &out) : out(out) {
            queue = dispatch_queue_create("hello-fswatcher", DISPATCH_QUEUE_SERIAL);
        }

        ~Impl() {
            stop();
            dispatch_release(queue);
        }

        Output &out;
        dispatch_queue_t queue;
        FSEventStreamRef stream = nullptr;

        std::mutex mutex;
        std::vector<Root> roots;

        void stop() {
            if (!stream) {
                return;
            }
            FSEventStreamStop(stream);
            FSEventStreamInvalidate(stream);
            FSEventStreamRelease(stream);
            stream = nullptr;
            // All pending callbacks of the old stream complete before this returns.
            dispatch_sync_f(queue, nullptr, [](void *) {});
        }

        static std::string given(const Root &root, const std::string &resolved) {
            return root.given + resolved.substr(root.resolved.size());
        }

        void handle(const std::string &path, FSEventStreamEventFlags flags) {
            std::lock_guard<std::mutex> lock(mutex);

            // Events were lost, either by the kernel or because this process fell behind.
            if (flags &
                (kFSEventStreamEventFlagKernelDropped | kFSEventStreamEventFlagUserDropped)) {
                for (const auto &root : roots) {
                    out.line("recdirty", root.given);
                }
                return;
            }

            for (const auto &root : roots) {
                // The root or one of its ancestors was renamed, removed or restored.
                if (flags & kFSEventStreamEventFlagRootChanged) {
                    if (path != root.resolved) {
                        continue;
                    }
                    if (isDirectory(root.resolved)) {
                        out.line("recdirty", root.given);
                    } else {
                        out.line("notfound", root.given);
                    }
                    continue;
                }

                if (!within(path, root.resolved)) {
                    continue;
                }

                // FSEvents coalesced the details and requests a full scan of the directory.
                if (flags & kFSEventStreamEventFlagMustScanSubDirs) {
                    out.line("recdirty", given(root, path));
                    continue;
                }

                if (path == root.resolved) {
                    out.line("dirty", root.given);
                    continue;
                }

                out.line("dirty", given(root, parentOf(path)));
                const bool arrived = (flags & (kFSEventStreamEventFlagItemCreated |
                                               kFSEventStreamEventFlagItemRenamed)) != 0;
                if ((flags & kFSEventStreamEventFlagItemIsDir) && arrived && isDirectory(path)) {
                    out.line("recdirty", given(root, path));
                }
            }
        }

        static void callback(ConstFSEventStreamRef, void *info, size_t count, void *paths,
                             const FSEventStreamEventFlags flags[], const FSEventStreamEventId[]) {
            auto *impl = static_cast<Impl *>(info);
            const auto *const *names = static_cast<const char *const *>(paths);
            for (size_t i = 0; i < count; ++i) {
                impl->handle(names[i], flags[i]);
            }
        }

        /// Starts one stream for \a paths , or returns false if FSEvents refuses.
        bool start(const std::vector<std::string> &paths) {
            if (paths.empty()) {
                return true;
            }
            CFMutableArrayRef array = CFArrayCreateMutable(
                kCFAllocatorDefault, CFIndex(paths.size()), &kCFTypeArrayCallBacks);
            for (const auto &path : paths) {
                CFStringRef string = CFStringCreateWithCString(kCFAllocatorDefault, path.c_str(),
                                                               kCFStringEncodingUTF8);
                CFArrayAppendValue(array, string);
                CFRelease(string);
            }

            FSEventStreamContext context = {0, this, nullptr, nullptr, nullptr};
            stream = FSEventStreamCreate(kCFAllocatorDefault, &Impl::callback, &context, array,
                                         kFSEventStreamEventIdSinceNow, 0.05,
                                         kFSEventStreamCreateFlagFileEvents |
                                             kFSEventStreamCreateFlagWatchRoot |
                                             kFSEventStreamCreateFlagNoDefer);
            CFRelease(array);
            if (!stream) {
                return false;
            }
            FSEventStreamSetDispatchQueue(stream, queue);
            if (!FSEventStreamStart(stream)) {
                FSEventStreamInvalidate(stream);
                FSEventStreamRelease(stream);
                stream = nullptr;
                return false;
            }
            return true;
        }
    };

    void prepareProcess() {
        // No dialogs are shown on this system, and streams have no text mode.
    }

    bool fileEventsAvailable() {
        return false;
    }

    // fileEvents is always false, because main() rejects --file-events on this system.
    Backend::Backend(Output &out, bool fileEvents) : m_impl(std::make_unique<Impl>(out)) {
        (void) fileEvents;
    }

    Backend::~Backend() = default;

    void Backend::follow(const std::vector<std::string> &roots) {
        auto &impl = *m_impl;
        impl.stop();

        std::vector<Impl::Root> next;
        std::vector<std::string> notFound;
        std::vector<std::string> unwatchable;
        for (const auto &given : roots) {
            char resolved[PATH_MAX];
            if (!realpath(given.c_str(), resolved) || !isDirectory(resolved)) {
                notFound.push_back(given);
                continue;
            }
            if (!isLocal(resolved)) {
                unwatchable.push_back(given);
                continue;
            }
            next.push_back({given, resolved});
        }

        {
            std::lock_guard<std::mutex> lock(impl.mutex);
            impl.roots = next;
        }

        std::vector<std::string> paths;
        for (const auto &root : next) {
            paths.push_back(root.resolved);
        }
        if (!impl.start(paths)) {
            for (const auto &root : next) {
                unwatchable.push_back(root.given);
            }
            std::lock_guard<std::mutex> lock(impl.mutex);
            impl.roots.clear();
        }

        for (const auto &root : notFound) {
            impl.out.line("notfound", root);
        }
        for (const auto &root : unwatchable) {
            impl.out.line("unwatchable", root);
        }
        impl.out.line("ok");
    }

}
