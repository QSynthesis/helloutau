#ifndef FSWATCHER_BACKEND_H
#define FSWATCHER_BACKEND_H

#include <memory>
#include <string>
#include <vector>

#include "Protocol.h"

namespace fswatcher {

    /// Sets the process up to run with nobody watching: standard input and output pass bytes
    /// as they are, and a failure ends the process rather than waiting on a dialog.
    void prepareProcess();

    /// Where the system's own change notifications are turned into the messages of Protocol.h .
    ///
    /// One per system. A system without one answers every root as \c unwatchable , which is
    /// true, and leaves the other side to look at the disk itself.
    class Backend {
    public:
        explicit Backend(Output &out);
        ~Backend();

        Backend(const Backend &) = delete;
        Backend &operator=(const Backend &) = delete;

        /// Replaces what is followed with \a roots , each a path in UTF-8, and answers \c ok .
        ///
        /// Returns once every root that can be followed is, so that a change made after the
        /// answer is reported.
        void follow(const std::vector<std::string> &roots);

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

}

#endif // FSWATCHER_BACKEND_H
