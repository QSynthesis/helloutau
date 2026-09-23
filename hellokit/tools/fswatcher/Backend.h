#ifndef FSWATCHER_BACKEND_H
#define FSWATCHER_BACKEND_H

#include <memory>
#include <string>
#include <vector>

#include "Protocol.h"

namespace fswatcher {

    /// Configures the process for unattended operation: standard input and output pass bytes
    /// unchanged, and a failure terminates the process instead of waiting on a dialog.
    void prepareProcess();

    /// Returns whether this system reports changes per entry, which the messages \c create ,
    /// \c delete and \c change require. False on macOS, where FSEvents reports directories.
    bool fileEventsAvailable();

    /// Translates the change notifications of the system into the messages of Protocol.h .
    ///
    /// One implementation per supported system, selected by CMake.
    class Backend {
    public:
        /// \param fileEvents whether to send \c create , \c delete and \c change in addition to
        ///        \c dirty . Requires fileEventsAvailable() .
        Backend(Output &out, bool fileEvents);
        ~Backend();

        Backend(const Backend &) = delete;
        Backend &operator=(const Backend &) = delete;

        /// Replaces the monitored roots with \a roots , each a UTF-8 path, and replies \c ok .
        ///
        /// Returns once every monitorable root is monitored, so that every change made after
        /// the reply is reported.
        void follow(const std::vector<std::string> &roots);

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

}

#endif // FSWATCHER_BACKEND_H
