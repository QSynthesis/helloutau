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

#include <hellokit/Config.h>

// hello-plugin-play: the preview program for classic plugins. A plugin such as PitTrace starts it
// under the name playinplugin.exe. See docs/PluginPlay.md.
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

    // The name of the pipe of the host without the process ID of the plugin
    constexpr wchar_t pipePrefix[] = L"\\\\.\\pipe\\helloutau-plugin-play-";

    // The name under which the original playinplugin.exe is kept on deployment
    constexpr wchar_t originalName[] = L"playinplugin-original.exe";

    // The time limit of the wait while every instance of the pipe of the host is busy
    constexpr DWORD busyTimeout = 5000;

    enum ExitCode {
        // The exit status of the original in every measured case
        Success = 0,
        NoHost = 1,
    };

    // The text has no column alignment and unbroken paragraphs, because a message box shows the
    // text in a proportional font and wraps the lines.
    constexpr char usage[] =
        "Usage: playinplugin.exe <input> [/m:<file>] [/o:<file>] [/noplay] [/nodeletewav] "
        "[/nodeletecache] [/debug]\n"
        "\n"
        "hello-plugin-play replaces the playinplugin.exe of a classic plugin under that name, so "
        "that the notes of the plugin are rendered and played in HelloUtau. The command line is "
        "that of the original playinplugin, and the switches are case-sensitive.\n"
        "\n"
        "<input>\n"
        "    The file of the notes, which is the first argument also if it has the form of a "
        "switch.\n"
        "/m:<file>\n"
        "    A file in the format of a plugin result, which is applied to the notes before the "
        "rendering. The input file is not changed.\n"
        "/o:<file>\n"
        "    The file into which the notes are rendered.\n"
        "/noplay\n"
        "    Renders the notes without playing them.\n"
        "/nodeletewav\n"
        "    Keeps the file of /o: after the preview, which is otherwise removed.\n"
        "/nodeletecache, /debug\n"
        "    Accepted and without effect, because HelloUtau creates none of the files that they "
        "concern in the original.\n"
        "/?, -h, --help\n"
        "    Shows this text, if it is the only argument.\n"
        "--version\n"
        "    Shows the version, if it is the only argument.\n"
        "\n"
        "If HelloUtau runs the plugin, the request is sent to HelloUtau, which renders and plays "
        "the notes. Otherwise the original program runs if it is kept beside this program as "
        "playinplugin-original.exe, and an error is shown if it is not.\n"
        "\n"
        "Exit status: 0 on success and for this text, the exit status of the original program "
        "if it runs, and 1 if neither HelloUtau nor the original program is available.\n";

    constexpr char version[] = "hello-plugin-play " HELLOKIT_VERSION "\n";

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
            // more elements.
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

    // Returns a connection to the pipe of the host, or std::nullopt if the other processes of the
    // job of this process have no pipe of a host.
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
    // original receives the command line that the plugin passed, and waits for it. Returns the
    // exit status of the original, or std::nullopt if the original is absent or cannot start.
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

    // Shows text, which is ASCII, on the standard output if it is redirected to a file or a pipe,
    // and in a message box otherwise, because a GUI program has no console of its own.
    void show(const char *text) {
        const auto output = GetStdHandle(STD_OUTPUT_HANDLE);
        const auto type =
            output && output != INVALID_HANDLE_VALUE ? GetFileType(output) : FILE_TYPE_UNKNOWN;
        const std::string bytes(text);
        if (type == FILE_TYPE_DISK || type == FILE_TYPE_PIPE) {
            DWORD written = 0;
            WriteFile(output, bytes.data(), DWORD(bytes.size()), &written, nullptr);
            return;
        }
        const std::wstring message(bytes.begin(), bytes.end());
        MessageBoxW(nullptr, message.c_str(), L"hello-plugin-play", MB_OK | MB_ICONINFORMATION);
    }

}

// The arguments are in UTF-8, from HelloKitWinMain.
int main(int argc, char *argv[]) {
    std::vector<std::wstring> arguments;
    for (int i = 1; i < argc; ++i) {
        arguments.push_back(std::filesystem::u8path(argv[i]).native());
    }
    // The original exits at once without arguments.
    if (arguments.empty()) {
        return Success;
    }
    // The help and the version only as the only argument, so that the first argument of a
    // longer command line is the input file as in the original. A file name cannot contain the
    // question mark of /?.
    if (arguments.size() == 1) {
        const std::wstring &argument = arguments.front();
        if (argument == L"/?" || argument == L"-h" || argument == L"--help") {
            show(usage);
            return Success;
        }
        if (argument == L"--version") {
            show(version);
            return Success;
        }
    }
    if (const auto pipe = connectToHost()) {
        // This program was started by the plugin, the foreground process, and may therefore
        // pass the right to set the foreground window to the host, so that the preview dialog
        // of the host appears in front instead of flashing in the taskbar.
        ULONG host = 0;
        if (GetNamedPipeServerProcessId(*pipe, &host)) {
            AllowSetForegroundWindow(host);
        }
        sendRequest(*pipe, message(parse(arguments)));
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
