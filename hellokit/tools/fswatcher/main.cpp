#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "Backend.h"
#include "Protocol.h"

// hello-fswatcher: reports which directories under the roots it is given may have changed.
//
// Started by hello::kit::FileSystemWatcher and spoken to over standard input and output, see
// Protocol.h . It takes no arguments and reads nothing but its input: what it follows comes from
// the process that started it and from nowhere else.

namespace {

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

int main() {
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

    // End of input is also what the watcher's process going away looks like, so that this one
    // never outlives it.
    return 0;
}
