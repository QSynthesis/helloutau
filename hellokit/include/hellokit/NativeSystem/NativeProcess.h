#ifndef HELLOKIT_NATIVESYSTEM_NATIVEPROCESS_H
#define HELLOKIT_NATIVESYSTEM_NATIVEPROCESS_H

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <hellokit/NativeSystem/NativeString.h>

namespace hello::kit {

    /// The current process and the processes that it starts, for the programs that link no Qt.
    /// See NativeString.h for the library.
    class NativeProcess {
    public:
        /// Returns whether the process runs with the privileges of the administrator: with an
        /// elevated token on Windows, which a program started with Run as administrator has,
        /// and as the user root elsewhere. On Windows a member of the Administrators group
        /// without elevation is not elevated, because User Account Control removes the
        /// privileges from its token.
        static bool isElevated();

        /// Starts \a program with \a arguments in \a workingDirectory, or in the current
        /// directory if it is empty, and does not wait for it. On Windows the arguments are
        /// quoted as CommandLineToArgvW() and the C runtime parse them.
        ///
        /// \return whether the process was started
        static bool start(const std::filesystem::path &program,
                          const std::vector<NativeString> &arguments,
                          const std::filesystem::path &workingDirectory = {});

        /// Runs \a program with \a arguments in the current directory and waits for it, the
        /// arguments quoted as for start().
        ///
        /// \return the exit status, 128 plus the number of the signal for a process ended by a
        ///         signal, as a shell reports it, or \c std::nullopt if the process does not
        ///         start
        static std::optional<int> run(const std::filesystem::path &program,
                                      const std::vector<NativeString> &arguments);

        /// Runs \a program with \a commandLine, which begins with the program name, and waits
        /// for it, the exit status as for run().
        ///
        /// On Windows the program receives \a commandLine as written rather than one quoted
        /// again from arguments, which matters to a program that parses its command line itself.
        /// Elsewhere \a commandLine is split into the arguments as a POSIX shell splits words,
        /// without any expansion: blanks separate the words, single quotes keep every character,
        /// double quotes keep every character except a backslash before \c $ , a backtick,
        /// \c " , \c \\ or a newline, and outside quotes a backslash keeps the next character.
        /// The first word becomes the name of the program in its arguments. No shell runs, so
        /// that \a commandLine cannot run a command.
        ///
        /// \return the exit status, or \c std::nullopt if the process does not start or
        ///         \a commandLine has an unterminated quotation
        static std::optional<int> runCommandLine(const std::filesystem::path &program,
                                                 NativeString commandLine);

    private:
        NativeProcess() = delete;
    };

}

#endif // HELLOKIT_NATIVESYSTEM_NATIVEPROCESS_H
