#ifndef FSWATCHER_PROTOCOL_H
#define FSWATCHER_PROTOCOL_H

#include <mutex>
#include <string>
#include <string_view>

/// What goes over the two pipes between this program and hello::kit::FileSystemWatcher .
///
/// Lines of UTF-8, one message each. A path is written with \c % , line feed and carriage
/// return as \c %25 , \c %0A and \c %0D , since a file name may hold a line break where the
/// system allows one, and a line protocol would otherwise take it for the end of the message.
///
/// **In**, from the watcher:
///
/// - \c roots , then one root per line, then \c # . Replaces everything followed so far.
/// - \c exit . End of input does the same, which is what the watcher's process ending looks like.
///
/// **Out**, to the watcher:
///
/// - \c greeting , first and once, so that the other side knows it started the right program.
/// - <tt>dirty \<path\></tt> : what is directly in this directory may have changed, its
///   listing or the contents of a file in it.
/// - <tt>recdirty \<path\></tt> : this directory and everything under it may have changed.
///   Sent for a directory that appeared, and for a whole root when events were lost.
/// - <tt>gone \<root\></tt> : the root is not there any more, or never was.
/// - <tt>unwatchable \<root\></tt> : nothing will be reported for this root, and it has to be
///   looked at some other way.
/// - \c ok : the roots last sent are followed, and a change made from now on is reported.
///
/// Every path out starts with a root exactly as it came in, so that the other side can tell
/// which of its roots a message is about by comparing text.
namespace fswatcher {

    inline constexpr char greeting[] = "hello-fswatcher 1";

    std::string escape(std::string_view path);
    std::string unescape(std::string_view text);

    /// Writes whole lines to standard output, from any thread.
    class Output {
    public:
        void line(std::string_view word);
        void line(std::string_view word, std::string_view path);

    private:
        std::mutex m_mutex;
    };

}

#endif // FSWATCHER_PROTOCOL_H
