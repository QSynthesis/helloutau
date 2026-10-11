#ifndef HELLOKIT_SUPPORT_RESTARTSCHEDULER_H
#define HELLOKIT_SUPPORT_RESTARTSCHEDULER_H

#include <QtCore/QStringList>

#include <hellokit/Support/HelloKitSupportGlobal.h>

namespace hello::kit {

    /// The restart of the application for the settings that take effect only at a start, such as
    /// the language.
    ///
    /// A setting that takes effect only after a restart calls requireRestart(). The application
    /// later asks the user whether to restart now and calls clearRestartRequired(). If the user
    /// agrees, the application calls scheduleRestart() and quits its event loop, and once the
    /// event loop has ended, calls relaunchIfScheduled().
    class HELLOKIT_SUPPORT_EXPORT RestartScheduler {
    public:
        /// Records that an applied setting takes effect only after a restart.
        static void requireRestart();

        /// Returns whether a setting applied since the last clearRestartRequired() takes effect
        /// only after a restart.
        static bool isRestartRequired();

        /// Clears the record of requireRestart(), as the question to the user does whether the
        /// user agrees or not.
        static void clearRestartRequired();

        /// Records that the program starts again once the event loop has ended. The caller quits
        /// the event loop.
        static void scheduleRestart();

        /// Returns whether a restart is scheduled.
        static bool isRestartScheduled();

        /// Starts a new process of the program with \a arguments, which exclude the program, if a
        /// restart is scheduled, and clears the schedule.
        ///
        /// \return Whether a restart was scheduled and the new process was started.
        static bool relaunchIfScheduled(const QStringList &arguments);
    };

}

#endif // HELLOKIT_SUPPORT_RESTARTSCHEDULER_H
