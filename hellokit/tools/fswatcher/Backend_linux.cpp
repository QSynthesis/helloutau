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

// How this follows a tree with inotify, which follows one directory and nothing under it.
//
// Every directory under a root is watched on its own, and a directory that appears is watched
// before it is reported, so that whatever landed in it before the watch is found by the look the
// other side takes. Watching holds nothing open in the way a handle does on Windows: a watched
// directory can be renamed and removed as ever.
//
// A root is also looked for from above. Every directory on the way down to it is watched for the
// one name that leads to the root, so that renaming a directory the root is in says the root is
// gone, which the root's own watch would never hear of.
//
// The number of watches a user may hold is limited, see /proc/sys/fs/inotify/max_user_watches .
// A root that runs into the limit is given up on and reported unwatchable, rather than followed
// in part.

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

        /// Whether \a path is \a base or somewhere under it.
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

        /// What one watch stands for.
        struct Watch {
            std::string path;
            /// Watched for the way down to a root rather than as part of one. Only the name
            /// that leads on counts there.
            bool above = false;
        };

        explicit Impl(Output &out) : out(out) {
            fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
            wake = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
            if (fd >= 0 && wake >= 0) {
                thread = std::thread([this] { run(); });
            }
        }

        ~Impl() {
            if (thread.joinable()) {
                const uint64_t one = 1;
                // Nothing to do if it fails: the thread is then left to end with the process.
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
        int fd = -1;
        int wake = -1;
        std::thread thread;

        // All below, read by the thread and changed by follow() .
        std::mutex mutex;
        std::vector<Root> roots;
        std::map<int, Watch> watches;
        // The last line said in the batch being read, so that a burst of the same says it once.
        // Only within one batch: the same news later is news again.
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

        /// Watches \a path for \a root , and nothing under it. False where the limit is reached,
        /// which is what gives the root up. Anything else that fails leaves out one directory,
        /// which a listing still shows.
        bool watchOne(const std::string &path, bool above) {
            const int wd = inotify_add_watch(fd, path.c_str(), above ? aboveMask : treeMask);
            if (wd < 0) {
                return errno != ENOSPC;
            }
            // The same directory under two roots is one watch, and the tree watch is the one
            // that says more.
            auto it = watches.find(wd);
            if (it == watches.end() || !above) {
                watches[wd] = Watch{path, above};
            }
            return true;
        }

        /// Watches \a path and every directory under it, without following links and without
        /// crossing onto another file system.
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

        /// Drops every watch at or under \a path .
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

        /// Watches the directories on the way down to \a root .
        void watchAbove(const Root &root) {
            std::string at = root.resolved;
            while (at != "/") {
                at = parentOf(at);
                watchOne(at, true);
            }
        }

        /// Starts following \a root afresh, and says so where that fails.
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
                // What was lost may include directories that appeared and are not watched.
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
                // The way down to a root, where only the root itself or what leads to it counts.
                // A watch from above names the root as a child of its parent, which reads as the
                // root itself, and has to be taken as news of it rather than from within it.
                if (watch.above || !within(path, root.resolved)) {
                    if (name.empty() || !within(root.resolved, path)) {
                        continue;
                    }
                    if (left) {
                        unwatchTree(root.resolved);
                        root.alive = false;
                        say("gone", root.given);
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

                // The root itself, gone without a word from above, as a root at / would be.
                if (path == root.resolved && (event.mask & (IN_DELETE_SELF | IN_MOVE_SELF))) {
                    unwatchTree(root.resolved);
                    root.alive = false;
                    say("gone", root.given);
                    continue;
                }
                if (event.mask & (IN_DELETE_SELF | IN_MOVE_SELF)) {
                    // Its parent says so as well, and with the name.
                    continue;
                }

                if (name.empty()) {
                    say("dirty", given(root, path));
                    continue;
                }
                say("dirty", given(root, watch.path));

                if (isDir && left) {
                    // A directory moved away keeps its watches, which would go on naming it
                    // where it no longer is.
                    unwatchTree(path);
                } else if (isDir && appeared) {
                    if (!watchTree(path)) {
                        unwatchTree(root.resolved);
                        root.alive = false;
                        say("unwatchable", root.given);
                        continue;
                    }
                    // Watched first and named after, so that what landed in it before the
                    // watch is found by the look this asks for.
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
        // Nothing puts up a dialog here, and streams have no text mode.
    }

    Backend::Backend(Output &out) : m_impl(std::make_unique<Impl>(out)) {
    }

    Backend::~Backend() = default;

    void Backend::follow(const std::vector<std::string> &roots) {
        auto &impl = *m_impl;
        std::vector<std::string> gone;
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
                    gone.push_back(given);
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

        for (const auto &root : gone) {
            impl.out.line("gone", root);
        }
        for (const auto &root : unwatchable) {
            impl.out.line("unwatchable", root);
        }
        impl.out.line("ok");
    }

}
