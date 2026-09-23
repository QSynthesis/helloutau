#include "Backend.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <map>
#include <mutex>
#include <set>
#include <thread>

#include <dirent.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

// Monitoring a tree with inotify, which monitors a single directory without its subdirectories.
//
// Every directory under a root receives its own watch, and a new directory is watched before it
// is reported, so that any entry created before the watch is found when the client examines the
// directory. A watch does not hold the directory open as a handle does on Windows, so a watched
// directory can still be renamed and deleted.
//
// Each root is also monitored from above. Every ancestor directory is watched for the one name
// that leads to the root, so that renaming an ancestor is reported as removal of the root, which
// the watch on the root itself would not detect.
//
// The number of watches per user is limited, see /proc/sys/fs/inotify/max_user_watches . A root
// that reaches the limit is abandoned and reported as unwatchable rather than monitored
// partially.

namespace fswatcher {

    namespace {

        constexpr uint32_t treeMask = IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO |
                                      IN_MODIFY | IN_CLOSE_WRITE | IN_ATTRIB | IN_DELETE_SELF |
                                      IN_MOVE_SELF | IN_ONLYDIR | IN_DONT_FOLLOW | IN_EXCL_UNLINK;

        constexpr uint32_t aboveMask = IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO |
                                       IN_DELETE_SELF | IN_MOVE_SELF | IN_ONLYDIR | IN_DONT_FOLLOW;

        bool isDirectory(const std::string &path) {
            struct stat info{};
            return lstat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
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
            bool alive = true;
        };

        /// The meaning of one watch.
        struct Watch {
            std::string path;
            /// Whether the watch is on an ancestor of a root rather than within a root. Only the
            /// name leading toward the root is relevant for such a watch.
            bool above = false;
        };

        Impl(Output &out, bool fileEvents) : out(out), fileEvents(fileEvents) {
            fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
            wake = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
            if (fd >= 0 && wake >= 0) {
                thread = std::thread([this] { run(); });
            }
        }

        ~Impl() {
            if (thread.joinable()) {
                const uint64_t one = 1;
                // A failure requires no handling, because the thread then ends with the process.
                if (write(wake, &one, sizeof(one)) < 0) {
                    thread.detach();
                } else {
                    thread.join();
                }
            }
            if (fd >= 0) {
                close(fd);
            }
            if (wake >= 0) {
                close(wake);
            }
        }

        Output &out;
        const bool fileEvents;
        int fd = -1;
        int wake = -1;
        std::thread thread;

        // All members below are read by the thread and modified by follow() .
        std::mutex mutex;
        std::vector<Root> roots;
        std::map<int, Watch> watches;
        // The last line written for the current batch, so that repeated identical events are
        // reported once. The deduplication applies only within one batch, because a later
        // identical event is a new change.
        std::string last;

        void say(const char *word, const std::string &path) {
            const std::string line = std::string(word) + '\n' + path;
            if (line == last) {
                return;
            }
            last = line;
            out.line(word, path);
        }

        static std::string given(const Root &root, const std::string &resolved) {
            return root.given + resolved.substr(root.resolved.size());
        }

        /// Watches \a path for \a root , without its subdirectories. Returns false if the watch
        /// limit is reached, which causes the root to be abandoned. Any other failure omits one
        /// directory, which remains visible in the listing of its parent.
        bool watchOne(const std::string &path, bool above) {
            const int wd = inotify_add_watch(fd, path.c_str(), above ? aboveMask : treeMask);
            if (wd < 0) {
                return errno != ENOSPC;
            }
            // A directory under two roots shares one watch, and the tree watch takes precedence
            // because it reports more events.
            auto it = watches.find(wd);
            if (it == watches.end() || !above) {
                watches[wd] = Watch{path, above};
            }
            return true;
        }

        /// Watches \a path and every directory under it, without following links and without
        /// crossing file system boundaries.
        bool watchTree(const std::string &path) {
            struct stat top{};
            if (lstat(path.c_str(), &top) != 0 || !S_ISDIR(top.st_mode)) {
                return true;
            }
            std::vector<std::string> pending{path};
            while (!pending.empty()) {
                const std::string at = pending.back();
                pending.pop_back();
                if (!watchOne(at, false)) {
                    return false;
                }
                DIR *listing = opendir(at.c_str());
                if (!listing) {
                    continue;
                }
                while (const dirent *entry = readdir(listing)) {
                    const std::string name = entry->d_name;
                    if (name == "." || name == "..") {
                        continue;
                    }
                    const std::string child = at + '/' + name;
                    struct stat info{};
                    if (lstat(child.c_str(), &info) == 0 && S_ISDIR(info.st_mode) &&
                        info.st_dev == top.st_dev) {
                        pending.push_back(child);
                    }
                }
                closedir(listing);
            }
            return true;
        }

        /// Removes every watch at or under \a path .
        void unwatchTree(const std::string &path) {
            for (auto it = watches.begin(); it != watches.end();) {
                if (!it->second.above && within(it->second.path, path)) {
                    inotify_rm_watch(fd, it->first);
                    it = watches.erase(it);
                } else {
                    ++it;
                }
            }
        }

        /// Watches the ancestor directories of \a root .
        void watchAbove(const Root &root) {
            std::string at = root.resolved;
            while (at != "/") {
                at = parentOf(at);
                watchOne(at, true);
            }
        }

        /// Starts monitoring \a root anew, and reports a failure.
        void take(Root &root) {
            unwatchTree(root.resolved);
            if (!watchTree(root.resolved)) {
                unwatchTree(root.resolved);
                root.alive = false;
                say("unwatchable", root.given);
            }
        }

        void handle(const inotify_event &event) {
            std::lock_guard<std::mutex> lock(mutex);

            if (event.mask & IN_Q_OVERFLOW) {
                // The lost events may include new directories that are not yet watched.
                for (auto &root : roots) {
                    if (root.alive && isDirectory(root.resolved)) {
                        take(root);
                        if (root.alive) {
                            say("recdirty", root.given);
                        }
                    }
                }
                return;
            }

            const auto it = watches.find(event.wd);
            if (it == watches.end()) {
                return;
            }
            if (event.mask & IN_IGNORED) {
                watches.erase(it);
                return;
            }

            const Watch watch = it->second;
            const std::string name = event.len > 0 ? std::string(event.name) : std::string();
            const std::string path = name.empty() ? watch.path : watch.path + '/' + name;
            const bool isDir = (event.mask & IN_ISDIR) != 0;
            const bool appeared = (event.mask & (IN_CREATE | IN_MOVED_TO)) != 0;
            const bool left = (event.mask & (IN_DELETE | IN_MOVED_FROM)) != 0;

            for (auto &root : roots) {
                // An ancestor watch, for which only the root or the path leading to it is
                // relevant. Such a watch reports the root as a child of its parent, which
                // resolves to the root path and must be treated as a change of the root itself
                // rather than of its contents.
                if (watch.above || !within(path, root.resolved)) {
                    if (name.empty() || !within(root.resolved, path)) {
                        continue;
                    }
                    if (left) {
                        unwatchTree(root.resolved);
                        root.alive = false;
                        say("notfound", root.given);
                    } else if (appeared && isDirectory(root.resolved)) {
                        root.alive = true;
                        take(root);
                        watchAbove(root);
                        if (root.alive) {
                            say("recdirty", root.given);
                        }
                    }
                    continue;
                }

                if (!root.alive) {
                    continue;
                }

                // The root itself was removed without an ancestor event, as for a root at /.
                if (path == root.resolved && (event.mask & (IN_DELETE_SELF | IN_MOVE_SELF))) {
                    unwatchTree(root.resolved);
                    root.alive = false;
                    say("notfound", root.given);
                    continue;
                }
                if (event.mask & (IN_DELETE_SELF | IN_MOVE_SELF)) {
                    // Its parent reports the removal as well, including the name.
                    continue;
                }

                if (name.empty()) {
                    say("dirty", given(root, path));
                    continue;
                }
                say("dirty", given(root, watch.path));
                if (fileEvents) {
                    if (appeared) {
                        say("create", given(root, path));
                    } else if (left) {
                        say("delete", given(root, path));
                    } else if (event.mask & (IN_MODIFY | IN_CLOSE_WRITE | IN_ATTRIB)) {
                        say("change", given(root, path));
                    }
                }

                if (isDir && left) {
                    // A moved directory keeps its watches, which would continue to report it
                    // under its former path.
                    unwatchTree(path);
                } else if (isDir && appeared) {
                    if (!watchTree(path)) {
                        unwatchTree(root.resolved);
                        root.alive = false;
                        say("unwatchable", root.given);
                        continue;
                    }
                    // Watched first and reported afterward, so that entries created before the
                    // watch are found when the client examines the directory.
                    say("recdirty", given(root, path));
                }
            }
        }

        void run() {
            alignas(inotify_event) char buffer[64 * 1024];
            pollfd fds[2] = {
                {fd,   POLLIN, 0},
                {wake, POLLIN, 0}
            };
            while (true) {
                if (poll(fds, 2, -1) < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    break;
                }
                if (fds[1].revents) {
                    break;
                }
                while (true) {
                    const ssize_t size = read(fd, buffer, sizeof(buffer));
                    if (size <= 0) {
                        break;
                    }
                    for (char *at = buffer; at < buffer + size;) {
                        const auto *event = reinterpret_cast<const inotify_event *>(at);
                        handle(*event);
                        at += sizeof(inotify_event) + event->len;
                    }
                    std::lock_guard<std::mutex> lock(mutex);
                    last.clear();
                }
            }
        }
    };

    void prepareProcess() {
        // No dialogs are shown on this system, and streams have no text mode.
    }

    bool fileEventsAvailable() {
        return true;
    }

    Backend::Backend(Output &out, bool fileEvents)
        : m_impl(std::make_unique<Impl>(out, fileEvents)) {
    }

    Backend::~Backend() = default;

    void Backend::follow(const std::vector<std::string> &roots) {
        auto &impl = *m_impl;
        std::vector<std::string> notFound;
        std::vector<std::string> unwatchable;
        {
            std::lock_guard<std::mutex> lock(impl.mutex);
            for (const auto &[wd, watch] : impl.watches) {
                inotify_rm_watch(impl.fd, wd);
            }
            impl.watches.clear();
            impl.roots.clear();
            impl.last.clear();

            for (const auto &given : roots) {
                if (impl.fd < 0 || !impl.thread.joinable()) {
                    unwatchable.push_back(given);
                    continue;
                }
                char resolved[PATH_MAX];
                if (!realpath(given.c_str(), resolved) || !isDirectory(resolved)) {
                    notFound.push_back(given);
                    continue;
                }
                Impl::Root root{given, resolved, true};
                if (!impl.watchTree(root.resolved)) {
                    impl.unwatchTree(root.resolved);
                    unwatchable.push_back(given);
                    continue;
                }
                impl.watchAbove(root);
                impl.roots.push_back(root);
            }
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
