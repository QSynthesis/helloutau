#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>

// hello-plugin-play: the preview program for classic plugins, which a plugin such as PitTrace
// starts under the name playinplugin.exe. See docs/PluginPlay.md.
//
// The command line is that of the original playinplugin, as measured in
// docs/claude/playinplugin.md. The request goes through a named pipe to the HelloUtau instance
// that runs the plugin, which renders and plays the notes, and the program exits after the reply.
// While the preview dialog of the host is open, the program is therefore running, which a plugin
// may rely on.
//
// The host is found through the job object into which HelloUtau puts the process of the plugin.
// Every process that the plugin starts belongs to the same job, and the pipe of the host is named
// after the process of the plugin. Without a host, the original program runs if it is kept beside
// this program as playinplugin-original.exe, and a message box appears otherwise.
//
// The program depends only on the standard library and the system, and links the C++ runtime
// statically, because it runs in the folder of a plugin, where no library of the application is
// available.

namespace fs = std::filesystem;

namespace {

    // The name of the pipe of the host without the process number of the plugin
    constexpr wchar_t pipePrefix[] = L"\\\\.\\pipe\\helloutau-plugin-play-";

    // The name under which the original playinplugin.exe is kept on deployment
    constexpr wchar_t originalName[] = L"playinplugin-original.exe";

    // The time limit of the wait for a pipe instance of the host that is busy
    constexpr DWORD busyTimeout = 5000;

    enum ExitCode {
        // The exit status of the original in every measured case
        Success = 0,
        NoHost = 1,
    };

    // The request to the host, in the terms of the original command line
    struct Request {
        std::wstring input;
        std::optional<std::wstring> modify;
        std::optional<std::wstring> output;
        bool play = true;
        bool keepOutput = false;
    };

    // Returns path as an absolute path against the working directory, or unchanged if the
    // conversion fails.
    std::wstring absolute(const std::wstring &path) {
        std::error_code ec;
        const auto result = fs::absolute(fs::path(path), ec);
        return ec ? path : result.native();
    }

    // Returns the request of arguments, which must not be empty. As in the original:
    //
    // - The first argument is the input file, also if it has the form of a switch.
    // - /m: and /o: are prefixes. An empty value counts as absent.
    // - The switches match exactly and case-sensitively.
    // - Unknown arguments are ignored.
    Request parse(const std::vector<std::wstring> &arguments) {
        Request request;
        request.input = absolute(arguments.front());
        for (size_t i = 1; i < arguments.size(); ++i) {
            const auto &argument = arguments[i];
            const auto valueOf = [&]() -> std::optional<std::wstring> {
                const auto value = argument.substr(3);
                if (value.empty()) {
                    return std::nullopt;
                }
                return absolute(value);
            };
            if (argument.compare(0, 3, L"/m:") == 0) {
                request.modify = valueOf();
            } else if (argument.compare(0, 3, L"/o:") == 0) {
                request.output = valueOf();
            } else if (argument == L"/noplay") {
                request.play = false;
            } else if (argument == L"/nodeletewav") {
                request.keepOutput = true;
            }
            // /nodeletecache and /debug concern the files of the original renderer, which the
            // host does not create.
        }
        return request;
    }

    std::string utf8(const std::wstring &text) {
        if (text.empty()) {
            return {};
        }
        const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0,
                                             nullptr, nullptr);
        std::string out(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), out.data(), size, nullptr,
                            nullptr);
        return out;
    }

    // Appends text to json as a JSON string.
    void appendString(std::string &json, const std::wstring &text) {
        constexpr char hex[] = "0123456789abcdef";
        json += '"';
        for (const char c : utf8(text)) {
            const auto byte = static_cast<unsigned char>(c);
            if (c == '"' || c == '\\') {
                json += '\\';
                json += c;
            } else if (byte < 0x20) {
                json += "\\u00";
                json += hex[byte >> 4];
                json += hex[byte & 0xf];
            } else {
                json += c;
            }
        }
        json += '"';
    }

    void appendOptional(std::string &json, const std::optional<std::wstring> &text) {
        if (text) {
            appendString(json, *text);
        } else {
            json += "null";
        }
    }

    // Returns the request as one line of JSON in UTF-8, the message format of docs/PluginPlay.md.
    std::string message(const Request &request) {
        std::string json = "{\"input\": ";
        appendString(json, request.input);
        json += ", \"modify\": ";
        appendOptional(json, request.modify);
        json += ", \"output\": ";
        appendOptional(json, request.output);
        json += ", \"play\": ";
        json += request.play ? "true" : "false";
        json += ", \"keepOutput\": ";
        json += request.keepOutput ? "true" : "false";
        json += "}\n";
        return json;
    }

    // Returns the processes of the job object of this process, or an empty list if the process
    // belongs to no job. A null job handle denotes the job of the calling process.
    std::vector<DWORD> jobProcesses() {
        BOOL inJob = FALSE;
        if (!IsProcessInJob(GetCurrentProcess(), nullptr, &inJob) || !inJob) {
            return {};
        }
        for (DWORD capacity = 64; capacity <= 65536; capacity *= 4) {
            // The structure ends with an array of one element. The buffer has room for capacity
            // elements more.
            std::vector<ULONG_PTR> buffer(
                sizeof(JOBOBJECT_BASIC_PROCESS_ID_LIST) / sizeof(ULONG_PTR) + capacity);
            const auto list = reinterpret_cast<JOBOBJECT_BASIC_PROCESS_ID_LIST *>(buffer.data());
            if (QueryInformationJobObject(nullptr, JobObjectBasicProcessIdList, list,
                                          DWORD(buffer.size() * sizeof(ULONG_PTR)), nullptr)) {
                std::vector<DWORD> processes;
                for (DWORD i = 0; i < list->NumberOfProcessIdsInList; ++i) {
                    processes.push_back(DWORD(list->ProcessIdList[i]));
                }
                return processes;
            }
            if (GetLastError() != ERROR_MORE_DATA) {
                break;
            }
        }
        return {};
    }

    // Returns a connection to the pipe of the host, or std::nullopt if no other process of the job
    // of this process has a pipe of a host.
    std::optional<HANDLE> connectToHost() {
        for (const auto process : jobProcesses()) {
            if (process == GetCurrentProcessId()) {
                continue;
            }
            const auto name = pipePrefix + std::to_wstring(process);
            for (int attempt = 0; attempt < 2; ++attempt) {
                const auto pipe = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                                              nullptr, OPEN_EXISTING, 0, nullptr);
                if (pipe != INVALID_HANDLE_VALUE) {
                    return pipe;
                }
                if (GetLastError() != ERROR_PIPE_BUSY ||
                    !WaitNamedPipeW(name.c_str(), busyTimeout)) {
                    break;
                }
            }
        }
        return std::nullopt;
    }

    // Sends the request and waits for the line of the reply, or for the end of the connection if
    // the host closes it first. There is no time limit, because the reply follows the closing of
    // the preview dialog. The reply is not interpreted, because the host shows a failure itself
    // and the exit status is the same in every case.
    void sendRequest(HANDLE pipe, const std::string &request) {
        DWORD written = 0;
        if (!WriteFile(pipe, request.data(), DWORD(request.size()), &written, nullptr)) {
            return;
        }
        char c = 0;
        DWORD read = 0;
        while (ReadFile(pipe, &c, 1, &read, nullptr) && read == 1 && c != '\n') {
        }
    }

    // Returns the arguments of the command line of this process as written, after the program
    // name. CommandLineToArgvW() ends a quoted program name at the next quotation mark and an
    // unquoted one at the first space or tab, without escapes.
    std::wstring argumentsAsWritten() {
        const std::wstring line = GetCommandLineW();
        size_t end = 0;
        if (!line.empty() && line.front() == L'"') {
            end = line.find(L'"', 1);
            end = end == std::wstring::npos ? line.size() : end + 1;
        } else {
            end = std::min(line.find_first_of(L" \t"), line.size());
        }
        return line.substr(end);
    }

    // Runs the original program beside this program with the arguments as written, so that the
    // original receives the command line that the plugin passed, and waits for it.
    //
    // \return the exit status of the original, or std::nullopt if the original is absent or
    //         cannot start
    std::optional<int> runOriginal() {
        std::wstring module(MAX_PATH, L'\0');
        for (;;) {
            const auto length = GetModuleFileNameW(nullptr, module.data(), DWORD(module.size()));
            if (length == 0) {
                return std::nullopt;
            }
            if (length < module.size()) {
                module.resize(length);
                break;
            }
            module.resize(module.size() * 2);
        }
        const auto original = fs::path(module).parent_path() / originalName;
        std::error_code ec;
        if (!fs::is_regular_file(original, ec)) {
            return std::nullopt;
        }
        // A file name cannot contain a quotation mark, which therefore needs no escape here.
        auto commandLine = L"\"" + original.native() + L"\"" + argumentsAsWritten();
        STARTUPINFOW startup = {};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process = {};
        if (!CreateProcessW(original.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0,
                            nullptr, nullptr, &startup, &process)) {
            return std::nullopt;
        }
        CloseHandle(process.hThread);
        WaitForSingleObject(process.hProcess, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(process.hProcess, &code);
        CloseHandle(process.hProcess);
        return int(code);
    }

}

int wmain(int argc, wchar_t *argv[]) {
    // The original exits at once without arguments.
    if (argc < 2) {
        return Success;
    }
    if (const auto pipe = connectToHost()) {
        sendRequest(*pipe, message(parse(std::vector<std::wstring>(argv + 1, argv + argc))));
        CloseHandle(*pipe);
        return Success;
    }
    if (const auto code = runOriginal()) {
        return *code;
    }
    MessageBoxW(nullptr,
                L"The preview of this plugin requires HelloUtau, and is available only if "
                L"HelloUtau runs the plugin. The original playinplugin.exe is not available as "
                L"playinplugin-original.exe in this folder.",
                L"playinplugin", MB_OK | MB_ICONERROR);
    return NoHost;
}
