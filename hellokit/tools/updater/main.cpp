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

#include <hellokit/Config.h>
#include <hellokit/NativeSystem/NativeMessageBox.h>
#include <hellokit/NativeSystem/NativeString.h>

// hello-updater: replaces the installation directory of a portable application with the directory
// of the new version.
//
// The application extracts the new version into a directory beside the installation directory,
// copies this program out of the installation directory, starts it and quits. This program waits
// for the application to exit, renames the installation directory aside, renames the new directory
// into its place, and removes the old directory. If a step fails, the installation directory is
// restored. Both renames are on one volume, so that each replaces a whole directory at once.
//
// The program depends only on the standard library, the system and static libraries of this
// repository, and links the C++ runtime statically, because the copy runs where no library of the
// application is available. It has no console on Windows. Its messages go to the log file, which
// the application reads at its next start. A failed restoration leaves no application to start,
// and is therefore also shown in a message box of HelloKitNativeSystem if --show-errors is given,
// as is the usage for invalid arguments. The texts of --help and --version are written to the
// standard output, on Windows only if it is redirected to a file or a pipe, and appear in a
// message box otherwise.

namespace fs = std::filesystem;
namespace kit = hello::kit;

namespace {

    using kit::NativeString;

    enum ExitCode {
        Success = 0,
        UsageError = 1,
        WaitTimeout = 2,
        // The installation directory is unchanged.
        ReplacementFailed = 3,
        // The installation directory is missing, and the old directory remains beside it.
        RestorationFailed = 4,
    };

    // The text has no column alignment and unbroken paragraphs, because a message box on Windows
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
        "    Shows a message box if the restoration fails or the arguments are invalid.\n"
        "--help\n"
        "    Shows this text and exits.\n"
        "--version\n"
        "    Shows the version and exits.\n"
        "\n"
        "Exit status: 0 on success, 1 for invalid arguments, 2 if the process did not exit in "
        "time, 3 if the replacement failed and the old directory was restored, 4 if the "
        "restoration also failed.\n";

    constexpr char version[] = "hello-updater " HELLOKIT_VERSION "\n";

    // The text that the arguments request instead of an update: none, the usage for --help, or
    // the version for --version
    enum class Information { None, Help, Version };

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
            if (c < HELLOKIT_NATIVE('0') || c > HELLOKIT_NATIVE('9')) {
                return std::nullopt;
            }
            result = result * 10 + (c - HELLOKIT_NATIVE('0'));
        }
        return result;
    }

    // Returns the options, or std::nullopt if the arguments are invalid or request a text, which
    // is then set in information.
    std::optional<Options> parse(const std::vector<NativeString> &arguments,
                                 Information *information) {
        Options options;
        for (size_t i = 0; i < arguments.size(); ++i) {
            const auto &name = arguments[i];
            if (name == HELLOKIT_NATIVE("--help")) {
                *information = Information::Help;
                return std::nullopt;
            }
            if (name == HELLOKIT_NATIVE("--version")) {
                *information = Information::Version;
                return std::nullopt;
            }
            if (name == HELLOKIT_NATIVE("--show-errors")) {
                options.showErrors = true;
                continue;
            }
            if (name == HELLOKIT_NATIVE("--")) {
                options.launchArguments.assign(arguments.begin() + i + 1, arguments.end());
                break;
            }
            if (i + 1 >= arguments.size()) {
                return std::nullopt;
            }
            const auto &value = arguments[++i];
            if (name == HELLOKIT_NATIVE("--target")) {
                options.target = value;
            } else if (name == HELLOKIT_NATIVE("--source")) {
                options.source = value;
            } else if (name == HELLOKIT_NATIVE("--launch")) {
                options.launch = value;
            } else if (name == HELLOKIT_NATIVE("--log")) {
                options.log = value;
            } else if (name == HELLOKIT_NATIVE("--pid")) {
                const auto pid = number(value);
                if (!pid || *pid > UINT32_MAX) {
                    return std::nullopt;
                }
                options.pid = std::uint32_t(*pid);
            } else if (name == HELLOKIT_NATIVE("--timeout")) {
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

#ifdef _WIN32
    // Returns whether the standard output is redirected to a file or a pipe. Windows has no
    // console for this program otherwise.
    bool outputRedirected() {
        const auto output = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!output || output == INVALID_HANDLE_VALUE) {
            return false;
        }
        const auto type = GetFileType(output);
        return type == FILE_TYPE_DISK || type == FILE_TYPE_PIPE;
    }
#endif

    // Shows the text that information requests, on the standard output if it is redirected, and
    // in a message box on Windows otherwise.
    void showInformation(Information information) {
        const auto text = information == Information::Help ? usage : version;
#ifdef _WIN32
        if (!outputRedirected() && kit::NativeMessageBox::show(
                                       "hello-updater", text, kit::NativeMessageBox::Information)) {
            return;
        }
#endif
        std::fputs(text, stdout);
        std::fflush(stdout);
    }

    // Shows the usage for invalid arguments: in a message box only with --show-errors, and on
    // the standard error otherwise or if no message box can be shown, so that a caller that waits
    // for the exit is not blocked.
    void showUsageError(bool showErrors) {
        if (showErrors &&
            kit::NativeMessageBox::show("hello-updater", usage, kit::NativeMessageBox::Critical)) {
            return;
        }
        std::fputs(usage, stderr);
    }

    int run(const std::vector<NativeString> &arguments) {
        auto information = Information::None;
        const auto parsed = parse(arguments, &information);
        if (!parsed) {
            if (information != Information::None) {
                showInformation(information);
                return Success;
            }
            // The arguments are searched without the parser, because the parser has rejected
            // them.
            const auto showErrors = std::find(arguments.begin(), arguments.end(),
                                              HELLOKIT_NATIVE("--show-errors")) != arguments.end();
            showUsageError(showErrors);
            return UsageError;
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
        old += HELLOKIT_NATIVE(".old");

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
                if (options.showErrors) {
                    const auto message = "The update failed, and the application could not be "
                                         "restored. It is left in the folder\n\n" +
                                         text(old) + "\n\nRename the folder to\n\n" + text(target);
                    kit::NativeMessageBox::show("Update", message.c_str(),
                                                kit::NativeMessageBox::Critical);
                }
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

// The arguments are in UTF-8, on Windows from HelloKitWinMain.
int main(int argc, char *argv[]) {
    std::vector<NativeString> arguments;
    for (int i = 1; i < argc; ++i) {
        arguments.push_back(kit::nativeOf(argv[i]));
    }
    return run(arguments);
}
