#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <cerrno>
#  include <csignal>
#  include <spawn.h>
#  include <sys/types.h>
extern char **environ;
#endif

// hello-updater: replaces the installation directory of a portable application with a new one.
//
// The application extracts the new version into a directory beside the installation directory,
// copies this program out of the installation directory, starts it and quits. This program waits
// for the application to exit, renames the installation directory aside, renames the new
// directory into its place, and removes the old one. If a step fails, the installation directory
// is restored. Both renames are on one volume, so that each replaces a whole directory at once.
//
// The program depends only on the standard library and the system, and links the C++ runtime
// statically, because the copy runs where no library of the application is available. It has no
// console on Windows. Its messages go to the log file, which the application reads at its next
// start. A failed restoration leaves no application to start, and is therefore also shown in a
// message box on Windows if --show-errors is given, as is the usage for invalid arguments. The
// usage for --help appears in a message box on Windows regardless.

namespace fs = std::filesystem;

namespace {

#ifdef _WIN32
    using NativeString = std::wstring;
#  define NATIVE(text) L##text
#else
    using NativeString = std::string;
#  define NATIVE(text) text
#endif

    enum ExitCode {
        Success = 0,
        UsageError = 1,
        WaitTimeout = 2,
        // The installation directory is unchanged.
        ReplacementFailed = 3,
        // The installation directory is missing, and the old one remains beside it.
        RestorationFailed = 4,
    };

    // Without column alignment and with unbroken paragraphs, because a message box on Windows
    // shows the text in a proportional font and wraps the lines.
    constexpr char usage[] =
        "Usage: hello-updater --target <dir> --source <dir> [options]\n"
        "\n"
        "Replaces the directory --target with the directory --source, both on one volume. The "
        "old directory is renamed to <target>.old and removed after the replacement. If the "
        "replacement fails, the old directory is restored.\n"
        "\n"
        "--target <dir>\n"
        "    The installation directory.\n"
        "--source <dir>\n"
        "    The new directory, usually <target>.new.\n"
        "--pid <number>\n"
        "    The process to wait for before the replacement.\n"
        "--timeout <seconds>\n"
        "    The time limit of the wait, 60 by default.\n"
        "--launch <program>\n"
        "    The program to start afterwards, also if the replacement fails and the old "
        "directory is restored. The arguments after -- are passed to it.\n"
        "--log <file>\n"
        "    The file to which the messages are appended in UTF-8.\n"
        "--show-errors\n"
        "    Shows a message box on Windows if the restoration fails or the arguments are "
        "invalid.\n"
        "--help\n"
        "    Shows this text and exits.\n"
        "\n"
        "Exit status: 0 on success, 1 for invalid arguments, 2 if the process did not exit in "
        "time, 3 if the replacement failed and the old directory was restored, 4 if the "
        "restoration also failed.\n";

    // A rename fails while another process holds a file in the directory open, which an
    // antivirus scanner or a file manager does for a moment.
    constexpr int renameAttempts = 50;
    constexpr auto renameInterval = std::chrono::milliseconds(200);

    struct Options {
        fs::path target;
        fs::path source;
        std::optional<std::uint32_t> pid;
        int timeout = 60;
        fs::path launch;
        std::vector<NativeString> launchArguments;
        fs::path log;
        bool showErrors = false;
    };

    class Log {
    public:
        explicit Log(const fs::path &path) {
            if (!path.empty()) {
                m_file.open(path, std::ios::app | std::ios::binary);
            }
        }

        void write(const std::string &message) {
            if (m_file) {
                m_file << "hello-updater: " << message << '\n';
                m_file.flush();
            }
        }

    private:
        std::ofstream m_file;
    };

    std::string text(const fs::path &path) {
        const auto utf8 = path.u8string();
        return std::string(utf8.begin(), utf8.end());
    }

    std::optional<long long> number(const NativeString &value) {
        if (value.empty() || value.size() > 10) {
            return std::nullopt;
        }
        long long result = 0;
        for (const auto c : value) {
            if (c < NATIVE('0') || c > NATIVE('9')) {
                return std::nullopt;
            }
            result = result * 10 + (c - NATIVE('0'));
        }
        return result;
    }

    // Returns the options, or std::nullopt if the arguments are invalid.
    std::optional<Options> parse(const std::vector<NativeString> &arguments, bool *help) {
        Options options;
        for (size_t i = 0; i < arguments.size(); ++i) {
            const auto &name = arguments[i];
            if (name == NATIVE("--help")) {
                *help = true;
                return std::nullopt;
            }
            if (name == NATIVE("--show-errors")) {
                options.showErrors = true;
                continue;
            }
            if (name == NATIVE("--")) {
                options.launchArguments.assign(arguments.begin() + i + 1, arguments.end());
                break;
            }
            if (i + 1 >= arguments.size()) {
                return std::nullopt;
            }
            const auto &value = arguments[++i];
            if (name == NATIVE("--target")) {
                options.target = value;
            } else if (name == NATIVE("--source")) {
                options.source = value;
            } else if (name == NATIVE("--launch")) {
                options.launch = value;
            } else if (name == NATIVE("--log")) {
                options.log = value;
            } else if (name == NATIVE("--pid")) {
                const auto pid = number(value);
                if (!pid || *pid > UINT32_MAX) {
                    return std::nullopt;
                }
                options.pid = std::uint32_t(*pid);
            } else if (name == NATIVE("--timeout")) {
                const auto timeout = number(value);
                if (!timeout || *timeout > 3600) {
                    return std::nullopt;
                }
                options.timeout = int(*timeout);
            } else {
                return std::nullopt;
            }
        }
        if (options.target.empty() || options.source.empty()) {
            return std::nullopt;
        }
        return options;
    }

    // Returns whether the process has exited within the time limit. A process that does not
    // exist has exited.
    bool waitForExit(std::uint32_t pid, int timeout) {
#ifdef _WIN32
        const auto process = OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (!process) {
            return GetLastError() == ERROR_INVALID_PARAMETER;
        }
        const auto result = WaitForSingleObject(process, DWORD(timeout) * 1000);
        CloseHandle(process);
        return result == WAIT_OBJECT_0;
#else
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout);
        while (kill(pid_t(pid), 0) == 0 || errno == EPERM) {
            if (std::chrono::steady_clock::now() >= deadline) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return true;
#endif
    }

    bool renameWithRetries(const fs::path &from, const fs::path &to, Log &log) {
        std::error_code ec;
        for (int attempt = 1;; ++attempt) {
            fs::rename(from, to, ec);
            if (!ec) {
                return true;
            }
            if (attempt == renameAttempts) {
                log.write("cannot rename " + text(from) + " to " + text(to) + ": " + ec.message());
                return false;
            }
            std::this_thread::sleep_for(renameInterval);
        }
    }

#ifdef _WIN32
    // Appends argument to commandLine as CommandLineToArgvW() and the C runtime parse it: in
    // quotation marks, with each quotation mark escaped and the backslashes before a quotation
    // mark doubled.
    void appendArgument(std::wstring &commandLine, const std::wstring &argument) {
        if (!commandLine.empty()) {
            commandLine += L' ';
        }
        commandLine += L'"';
        size_t backslashes = 0;
        for (const auto c : argument) {
            if (c == L'\\') {
                ++backslashes;
                continue;
            }
            if (c == L'"') {
                commandLine.append(backslashes * 2 + 1, L'\\');
            } else {
                commandLine.append(backslashes, L'\\');
            }
            backslashes = 0;
            commandLine += c;
        }
        // The closing quotation mark follows, so that the trailing backslashes are doubled.
        commandLine.append(backslashes * 2, L'\\');
        commandLine += L'"';
    }
#endif

    bool launch(const fs::path &program, const std::vector<NativeString> &arguments) {
#ifdef _WIN32
        std::wstring commandLine;
        appendArgument(commandLine, program.native());
        for (const auto &argument : arguments) {
            appendArgument(commandLine, argument);
        }
        STARTUPINFOW startup = {};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process = {};
        const auto directory = program.parent_path();
        if (!CreateProcessW(program.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0,
                            nullptr, directory.c_str(), &startup, &process)) {
            return false;
        }
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return true;
#else
        std::string path = program.native();
        auto values = arguments;
        std::vector<char *> argv = {path.data()};
        for (auto &value : values) {
            argv.push_back(value.data());
        }
        argv.push_back(nullptr);
        pid_t pid = 0;
        return posix_spawn(&pid, path.c_str(), nullptr, nullptr, argv.data(), environ) == 0;
#endif
    }

    // Shows the usage for --help, or for invalid arguments if help is false. Windows has no
    // console for this program, and the usage appears in a message box there. Invalid arguments
    // appear in a message box only with --show-errors, and on the standard error otherwise, so
    // that a caller that waits for the exit is not blocked.
    void showUsage(bool help, bool showErrors) {
#ifdef _WIN32
        if (help || showErrors) {
            // The usage is ASCII.
            const std::wstring message(usage, usage + sizeof(usage) - 1);
            MessageBoxW(nullptr, message.c_str(), L"hello-updater",
                        MB_OK | (help ? MB_ICONINFORMATION : MB_ICONERROR));
            return;
        }
#else
        if (help) {
            std::fputs(usage, stdout);
            return;
        }
#endif
        std::fputs(usage, stderr);
    }

    int run(const std::vector<NativeString> &arguments) {
        bool help = false;
        const auto parsed = parse(arguments, &help);
        if (!parsed) {
            // Searched without the parser, which has rejected the arguments
            const auto showErrors = std::find(arguments.begin(), arguments.end(),
                                              NATIVE("--show-errors")) != arguments.end();
            showUsage(help, showErrors);
            return help ? Success : UsageError;
        }
        const auto &options = *parsed;
        Log log(options.log);

        if (options.pid && !waitForExit(*options.pid, options.timeout)) {
            log.write("the process " + std::to_string(*options.pid) + " did not exit");
            return WaitTimeout;
        }

        const auto target = fs::absolute(options.target);
        const auto source = fs::absolute(options.source);
        auto old = target;
        old += NATIVE(".old");

        // A working directory inside the installation directory prevents its rename on Windows.
        std::error_code ec;
        fs::current_path(target.parent_path(), ec);

        const auto finish = [&](int code) {
            if (!options.launch.empty() && !launch(options.launch, options.launchArguments)) {
                log.write("cannot start " + text(options.launch));
            }
            return code;
        };

        if (!fs::is_directory(target, ec) || !fs::is_directory(source, ec)) {
            log.write("the directory " + text(target) + " or " + text(source) + " is missing");
            return finish(ReplacementFailed);
        }
        if (fs::exists(old, ec)) {
            fs::remove_all(old, ec);
            if (ec) {
                log.write("cannot remove " + text(old) + ": " + ec.message());
                return finish(ReplacementFailed);
            }
        }
        if (!renameWithRetries(target, old, log)) {
            return finish(ReplacementFailed);
        }
        if (!renameWithRetries(source, target, log)) {
            if (!renameWithRetries(old, target, log)) {
                log.write("the installation is left at " + text(old));
#ifdef _WIN32
                if (options.showErrors) {
                    const auto message = L"The update failed, and the application could not be "
                                         L"restored. It is left in the folder\n\n" +
                                         old.native() + L"\n\nRename the folder to\n\n" +
                                         target.native();
                    MessageBoxW(nullptr, message.c_str(), L"Update", MB_OK | MB_ICONERROR);
                }
#endif
                return RestorationFailed;
            }
            return finish(ReplacementFailed);
        }
        log.write("replaced " + text(target));
        // The new version starts before the removal, which takes longer than the renames.
        finish(Success);
        fs::remove_all(old, ec);
        if (ec) {
            // The application removes the remainder at its next start.
            log.write("cannot remove " + text(old) + ": " + ec.message());
        }
        return Success;
    }

}

#ifdef _WIN32
int wmain(int argc, wchar_t *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    return run(std::vector<NativeString>(argv + 1, argv + argc));
}
