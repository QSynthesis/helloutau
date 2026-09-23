#ifndef FSWATCHER_PROTOCOL_H
#define FSWATCHER_PROTOCOL_H

#include <mutex>
#include <string>
#include <string_view>

/// The protocol between this program and hello::kit::FileSystemWatcher over standard input and
/// output. The usage text in main.cpp repeats it and must be kept in sync.
///
/// Each message is one line of UTF-8 text. In paths, \c % , line feed and carriage return are
/// encoded as \c %25 , \c %0A and \c %0D , because a file name may contain a line break where
/// the system permits one, which a line-based protocol would otherwise read as the end of the
/// message.
///
/// **Input**, from the client:
///
/// - \c roots , followed by one root per line and a line containing only \c # . Replaces the set
///   of monitored roots.
/// - \c exit . End of input has the same effect, and occurs when the client process exits.
///
/// **Output**, to the client:
///
/// - \c greeting , sent once at startup, which lets the client verify that it started the
///   correct program.
/// - \c ok : the most recently received roots are monitored, and subsequent changes are
///   reported.
/// - <tt>dirty \<path\></tt> : the direct contents of the directory may have changed, either
///   its listing or the contents of a file in it.
/// - <tt>recdirty \<path\></tt> : the directory and its entire subtree may have changed. Sent
///   for a newly created directory, and for a root after events were lost.
/// - <tt>gone \<root\></tt> : the root does not exist.
/// - <tt>unwatchable \<root\></tt> : the root cannot be monitored, and changes to it must be
///   detected by other means.
/// - <tt>unknown \<line\></tt> : the input line was not recognized. Encoded as a path.
///
/// **Entry messages**, sent only if the program was started with \c --file-events , which is
/// available on Windows and Linux only:
///
/// - <tt>create \<path\></tt> : an entry appeared, by creation or by a rename or move into place.
/// - <tt>delete \<path\></tt> : an entry disappeared, by deletion or by a rename or move away.
/// - <tt>change \<path\></tt> : the contents or metadata of an entry may have changed.
///
/// The parent directory of every such entry is reported by \c dirty as well, so a client that
/// ignores these messages receives the same information as without the option. They are not
/// sent for a root, and not for changes covered by \c recdirty after events were lost. Like all
/// messages they are hints: an entry may be reported more than once, or after it has changed
/// again.
///
/// Every reported path begins with a root exactly as received, so that the client can identify
/// the root by string comparison. The remainder of the path uses the native separator.
namespace fswatcher {

    inline constexpr char greeting[] = "hello-fswatcher 1";

    std::string escape(std::string_view path);
    std::string unescape(std::string_view text);

    /// Writes complete lines to standard output. Safe to call from any thread.
    class Output {
    public:
        void line(std::string_view word);
        void line(std::string_view word, std::string_view path);

    private:
        std::mutex m_mutex;
    };

}

#endif // FSWATCHER_PROTOCOL_H
