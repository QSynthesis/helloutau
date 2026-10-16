#include "NativeProcess.h"

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>

namespace hello::kit {

    namespace {

        // Appends argument to commandLine as CommandLineToArgvW() and the C runtime parse it: in
        // quotation marks, with each quotation mark escaped and the backslashes before a
        // quotation mark doubled.
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

        std::wstring commandLineOf(const std::filesystem::path &program,
                                   const std::vector<NativeString> &arguments) {
            std::wstring commandLine;
            appendArgument(commandLine, program.native());
            for (const auto &argument : arguments) {
                appendArgument(commandLine, argument);
            }
            return commandLine;
        }

        // Starts the process, and returns its handle, or null if it does not start
        HANDLE startProcess(const std::filesystem::path &program, std::wstring &commandLine,
                            const std::filesystem::path &workingDirectory) {
            STARTUPINFOW startup = {};
            startup.cb = sizeof(startup);
            PROCESS_INFORMATION process = {};
            if (!CreateProcessW(program.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0,
                                nullptr,
                                workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
                                &startup, &process)) {
                return nullptr;
            }
            CloseHandle(process.hThread);
            return process.hProcess;
        }

    }

    bool NativeProcess::isElevated() {
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
            return false;
        }
        TOKEN_ELEVATION elevation = {};
        DWORD size = 0;
        const bool elevated =
            GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) &&
            elevation.TokenIsElevated != 0;
        CloseHandle(token);
        return elevated;
    }

    bool NativeProcess::start(const std::filesystem::path &program,
                              const std::vector<NativeString> &arguments,
                              const std::filesystem::path &workingDirectory) {
        auto commandLine = commandLineOf(program, arguments);
        const auto process = startProcess(program, commandLine, workingDirectory);
        if (!process) {
            return false;
        }
        CloseHandle(process);
        return true;
    }

    std::optional<int> NativeProcess::run(const std::filesystem::path &program,
                                          const std::vector<NativeString> &arguments) {
        return runCommandLine(program, commandLineOf(program, arguments));
    }

    std::optional<int> NativeProcess::runCommandLine(const std::filesystem::path &program,
                                                     NativeString commandLine) {
        const auto process = startProcess(program, commandLine, {});
        if (!process) {
            return std::nullopt;
        }
        WaitForSingleObject(process, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(process, &code);
        CloseHandle(process);
        return int(code);
    }

}
