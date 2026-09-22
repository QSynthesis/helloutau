#include <cstdio>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "Backend.h"
#include "Protocol.h"

// hello-fswatcher: reports directories under a set of roots that may have changed.
//
// Started by hello::kit::FileSystemWatcher, which communicates with it over standard input and
// output as specified in Protocol.h. The roots are received only on standard input, never from
// the command line, the environment or a file.

namespace {

    // Keep in sync with Protocol.h.
    constexpr char usage[] = R"(Usage: hello-fswatcher [--help]

Reports directories under a set of root directories that may have changed on disk. The program
is started by hello::kit::FileSystemWatcher and communicates over standard input and output. It
accepts no command-line arguments other than --help.

Encoding
  Each message is one line of UTF-8 text. In paths, '%', line feed and carriage return are
  encoded as %25, %0A and %0D.

Input (standard input)
  roots               Followed by one root path per line and a line containing only '#'.
                      Replaces the set of monitored roots.
  exit                Terminates the program. End of input has the same effect.

Output (standard output)
  hello-fswatcher 1   Greeting, sent once at startup. The number is the protocol version.
  ok                  The most recently received roots are monitored. Changes made after
                      this message are reported.
  dirty <path>        The direct contents of <path> may have changed: its listing or the
                      contents of a file in it.
  recdirty <path>     <path> and its entire subtree may have changed. Sent for a newly
                      created directory, and for a root after events were lost.
  gone <root>         <root> does not exist.
  unwatchable <root>  <root> cannot be monitored, and changes to it must be detected by
                      other means.
  unknown <line>      <line> was not a recognized input message. Encoded as a path.

Every reported path begins with one of the roots exactly as received, so that the client can
identify the root by string comparison. The remainder of the path uses the native separator of
the system. Reports are hints, not facts. A reported directory may
be unchanged, and changes may go unreported when the system drops events. A client must
rescan a reported directory and compare it with its previous state.

Example (Linux; lines marked > are input, lines marked < are output)
  < hello-fswatcher 1
  > roots
  > /home/user/voice/bank
  > #
  < ok
  < dirty /home/user/voice/bank/A3
  < recdirty /home/user/voice/bank/new
  > exit
)";

    bool readLine(std::string &line) {
        if (!std::getline(std::cin, line)) {
            return false;
        }
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        return true;
    }

}

int main(int argc, char **argv) {
    // Checked before anything is written to standard output, which is reserved for the protocol.
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (argument == "--help") {
            std::fputs(usage, stdout);
            return 0;
        }
        std::fprintf(stderr, "hello-fswatcher: unknown argument '%s'. See --help.\n", argv[i]);
        return 2;
    }

    fswatcher::prepareProcess();

    fswatcher::Output out;
    out.line(fswatcher::greeting);

    fswatcher::Backend backend(out);

    std::string line;
    while (readLine(line)) {
        if (line == "roots") {
            std::vector<std::string> roots;
            while (readLine(line) && line != "#") {
                roots.push_back(fswatcher::unescape(line));
            }
            backend.follow(roots);
        } else if (line == "exit") {
            break;
        } else if (!line.empty()) {
            out.line("unknown", line);
        }
    }

    // End of input also occurs when the client process exits, which ensures that this process
    // never outlives it.
    return 0;
}
