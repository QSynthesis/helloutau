#ifndef HELLOKIT_SUPPORT_EXITLAUNCHER_H
#define HELLOKIT_SUPPORT_EXITLAUNCHER_H

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Support/HelloKitSupportGlobal.h>

namespace hello::kit {

    /// The programs that start once the event loop of the application has ended, such as the
    /// application itself for a restart, or the installer of an update, which replaces the files
    /// of the application after it exits.
    ///
    /// The application creates one instance, as it creates one \c QCoreApplication, and reaches
    /// it with instance(). Each launch is scheduled under a key chosen by its caller, so that a
    /// launch scheduled again replaces the earlier one. Once the event loop has ended, the
    /// application calls launchAll().
    ///
    /// The instance also records whether an applied setting takes effect only after a restart,
    /// such as the language, so that the application offers the user a restart.
    class HELLOKIT_SUPPORT_EXPORT ExitLauncher {
    public:
        /// Creates the launcher. scheduleRestart() starts the program with \a restartArguments,
        /// which exclude the program. At most one instance exists at a time.
        explicit ExitLauncher(const QStringList &restartArguments = {});

        /// Destroys the launcher without starting the scheduled programs.
        ~ExitLauncher();

        /// Returns the instance, or \c nullptr if none exists.
        static ExitLauncher *instance();

        /// Schedules \a program with \a arguments under \a key, replacing the launch scheduled
        /// under \a key before.
        void setLaunch(const QString &key, const QString &program, const QStringList &arguments);

        /// Removes the launch scheduled under \a key.
        void removeLaunch(const QString &key);

        /// Returns whether a launch is scheduled under \a key.
        bool hasLaunch(const QString &key) const;

        /// The key of the restart
        static constexpr char restartKey[] = "restart";

        /// Schedules the program with the restart arguments under \c restartKey. The caller
        /// quits the event loop.
        void scheduleRestart();

        /// Returns whether a restart is scheduled.
        bool isRestartScheduled() const;

        /// Records that an applied setting takes effect only after a restart.
        void requireRestart();

        /// Returns whether a setting applied since the last clearRestartRequired() takes effect
        /// only after a restart.
        bool isRestartRequired() const;

        /// Clears the record of requireRestart(), as the question to the user does whether the
        /// user agrees or not.
        void clearRestartRequired();

        /// Starts each scheduled program detached, in the order of scheduling, and clears the
        /// schedule.
        ///
        /// \return the number of the programs that were started
        int launchAll();

    private:
        struct Launch {
            QString key;
            QString program;
            QStringList arguments;
        };

        QStringList m_restartArguments;
        QList<Launch> m_launches;
        bool m_restartRequired = false;

        Q_DISABLE_COPY(ExitLauncher)
    };

}

#endif // HELLOKIT_SUPPORT_EXITLAUNCHER_H
