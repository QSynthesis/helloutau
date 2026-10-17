#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <cerrno>
#  include <csignal>
#  include <sys/socket.h>
#  include <sys/types.h>
#  include <sys/un.h>
#  include <unistd.h>
#  ifdef __APPLE__
#    include <mach-o/dyld.h>
#  endif
#endif

#include <hellokit/Config.h>
#include <hellokit/NativeSystem/NativeMessageBox.h>
#include <hellokit/NativeSystem/NativeProcess.h>
#include <hellokit/NativeSystem/NativeString.h>

// hello-plugin-play: the preview program for classic plugins. A plugin such as PitTrace starts it
// under the name playinplugin.exe, or playinplugin on other systems. See docs/PluginPlay.md.
//
// The command line is that of the original playinplugin, as measured in
// docs/claude/playinplugin.md. The request goes to the HelloUtau instance that runs the plugin,
// which renders and plays the notes, and the program exits after the reply. While the preview
// dialog of the host is open, the program is therefore running, which a plugin may rely on.
//
// On Windows, the host is found through the job object into which HelloUtau puts the process of
// the plugin. Every process that the plugin starts belongs to the same job, and the named pipe of
// the host is named after the process of the plugin. On other systems, HelloUtau starts the plugin
// in a process group of its own, the ID of which is the process ID of the plugin, and the local
// socket of the host is named after it. Without a host, the original program runs if it is kept
// beside this program as playinplugin-original.exe, or playinplugin-original on other systems.
// An error is shown otherwise, in a message box of HelloKitNativeSystem, or on the standard error
// if no message box can be shown.
//
// The program depends only on the standard library, the system and static libraries of this
// repository, and links the C++ runtime statically, because it runs in the folder of a plugin,
// where no library of the application is available.

namespace fs = std::filesystem;
namespace kit = hello::kit;

namespace {

    using kit::NativeString;

#ifdef _WIN32
#  define PROGRAM_SUFFIX ".exe"
#else
#  define PROGRAM_SUFFIX ""
#endif

#ifdef _WIN32
    // The name of the pipe of the host without the process ID of the plugin
    constexpr wchar_t pipePrefix[] = L"\\\\.\\pipe\\helloutau-plugin-play-";

    // The time limit of the wait while every instance of the pipe of the host is busy
    constexpr DWORD busyTimeout = 5000;

    using Connection = HANDLE;
#else
    // The file name of the local socket of the host without the process ID of the plugin, the
    // name that ClassicPluginPreviewServer passes to QLocalServer
    constexpr char socketPrefix[] = "helloutau-plugin-play-";

    using Connection = int;
#endif

    // The name under which the original playinplugin is kept on deployment
    constexpr auto originalName = HELLOKIT_NATIVE("playinplugin-original") PROGRAM_SUFFIX;

    enum ExitCode {
        // The exit status of the original in every measured case
        Success = 0,
        NoHost = 1,
    };

    // The text has no column alignment and unbroken paragraphs, because a message box on Windows
    // shows the text in a proportional font and wraps the lines.
    constexpr char usage[] =
        "Usage: playinplugin" PROGRAM_SUFFIX " <input> [/m:<file>] [/o:<file>] [/noplay] "
        "[/nodeletewav] [/nodeletecache] [/debug]\n"
        "\n"
        "hello-plugin-play replaces the playinplugin" PROGRAM_SUFFIX " of a classic plugin under "
        "that name, so that the notes of the plugin are rendered and played in HelloUtau. The "
        "command line is that of the original playinplugin, and the switches are "
        "case-sensitive.\n"
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
        "playinplugin-original" PROGRAM_SUFFIX ", and an error is shown if it is not.\n"
        "\n"
        "Exit status: 0 on success and for this text, the exit status of the original program "
        "if it runs, and 1 if neither HelloUtau nor the original program is available.\n";

    constexpr char version[] = "hello-plugin-play " HELLOKIT_VERSION "\n";

    constexpr char noHost[] =
        "The preview of this plugin requires HelloUtau, and is available only if HelloUtau runs "
        "the plugin. The original playinplugin" PROGRAM_SUFFIX " is not available as "
        "playinplugin-original" PROGRAM_SUFFIX " in this folder.";

    // The request to the host, in the terms of the original command line
    struct Request {
        NativeString input;
        std::optional<NativeString> modify;
        std::optional<NativeString> output;
        bool play = true;
        bool keepOutput = false;
    };

    // Returns path as an absolute path against the working directory, or unchanged if the
    // conversion fails.
    NativeString absolute(const NativeString &path) {
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
    Request parse(const std::vector<NativeString> &arguments) {
        Request request;
        request.input = absolute(arguments.front());
        for (size_t i = 1; i < arguments.size(); ++i) {
            const auto &argument = arguments[i];
            const auto valueOf = [&]() -> std::optional<NativeString> {
                const auto value = argument.substr(3);
                if (value.empty()) {
                    return std::nullopt;
                }
                return absolute(value);
            };
            if (argument.compare(0, 3, HELLOKIT_NATIVE("/m:")) == 0) {
                request.modify = valueOf();
            } else if (argument.compare(0, 3, HELLOKIT_NATIVE("/o:")) == 0) {
                request.output = valueOf();
            } else if (argument == HELLOKIT_NATIVE("/noplay")) {
                request.play = false;
            } else if (argument == HELLOKIT_NATIVE("/nodeletewav")) {
                request.keepOutput = true;
            }
            // /nodeletecache and /debug concern the files of the original renderer, which the
            // host does not create.
        }
        return request;
    }

    // Appends text to json as a JSON string.
    void appendString(std::string &json, const NativeString &text) {
        constexpr char hex[] = "0123456789abcdef";
        json += '"';
        for (const char c : kit::utf8Of(text)) {
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

    void appendOptional(std::string &json, const std::optional<NativeString> &text) {
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

#ifdef _WIN32
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
    std::optional<Connection> connectToHost() {
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

    // Writes data, and returns whether every byte was written.
    bool writeAll(Connection connection, const std::string &data) {
        DWORD written = 0;
        return WriteFile(connection, data.data(), DWORD(data.size()), &written, nullptr) &&
               written == data.size();
    }

    // Reads one byte, and returns whether a byte was read.
    bool readByte(Connection connection, char &c) {
        DWORD read = 0;
        return ReadFile(connection, &c, 1, &read, nullptr) && read == 1;
    }

    void disconnect(Connection connection) {
        CloseHandle(connection);
    }
#else
    // Returns the directory of the local socket of the host, as QDir::tempPath() of Qt returns it
    // on Unix: TMPDIR, the temporary directory of the user on macOS, or /tmp, each without a
    // trailing slash. The plugin is expected to pass TMPDIR of HelloUtau unchanged.
    std::string socketDirectory() {
        std::string directory;
        if (const auto value = std::getenv("TMPDIR")) {
            directory = value;
        }
#  ifdef __APPLE__
        if (directory.empty()) {
            std::string buffer(confstr(_CS_DARWIN_USER_TEMP_DIR, nullptr, 0), '\0');
            if (!buffer.empty() &&
                confstr(_CS_DARWIN_USER_TEMP_DIR, buffer.data(), buffer.size())) {
                directory = buffer.c_str();
            }
        }
#  endif
        if (directory.empty()) {
            return "/tmp";
        }
        directory = absolute(fs::path(directory).lexically_normal().native());
        while (directory.size() > 1 && directory.back() == '/') {
            directory.pop_back();
        }
        return directory;
    }

    // Returns a connection to the socket of the host, or std::nullopt if the leader of the
    // process group of this process has no socket of a host. A program that leads its own
    // process group was not started by a plugin of HelloUtau.
    std::optional<Connection> connectToHost() {
        const auto group = getpgid(0);
        if (group <= 0 || group == getpid()) {
            return std::nullopt;
        }
        const auto path = socketDirectory() + '/' + socketPrefix + std::to_string(group);
        sockaddr_un address = {};
        address.sun_family = AF_UNIX;
        if (path.size() >= sizeof(address.sun_path)) {
            return std::nullopt;
        }
        std::copy(path.begin(), path.end(), address.sun_path);
        const int socket = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (socket < 0) {
            return std::nullopt;
        }
        if (::connect(socket, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0) {
            ::close(socket);
            return std::nullopt;
        }
        // A host that closes the connection first ends the write with an error rather than with
        // the signal, which would terminate this program. The disposition is set only here,
        // because an ignored signal is inherited by the original program.
        std::signal(SIGPIPE, SIG_IGN);
        return socket;
    }

    // Writes data, and returns whether every byte was written.
    bool writeAll(Connection connection, const std::string &data) {
        size_t done = 0;
        while (done < data.size()) {
            const auto written = ::write(connection, data.data() + done, data.size() - done);
            if (written < 0 && errno == EINTR) {
                continue;
            }
            if (written <= 0) {
                return false;
            }
            done += size_t(written);
        }
        return true;
    }

    // Reads one byte, and returns whether a byte was read.
    bool readByte(Connection connection, char &c) {
        for (;;) {
            const auto read = ::read(connection, &c, 1);
            if (read < 0 && errno == EINTR) {
                continue;
            }
            return read == 1;
        }
    }

    void disconnect(Connection connection) {
        ::close(connection);
    }
#endif

    // Sends the request and waits for the line of the reply, or for the end of the connection if
    // the host closes it first. There is no time limit, because the reply follows the closing of
    // the preview dialog. The reply is not interpreted, because the host shows a failure itself
    // and the exit status is the same in every case.
    void sendRequest(Connection connection, const std::string &request) {
        if (!writeAll(connection, request)) {
            return;
        }
        char c = 0;
        while (readByte(connection, c) && c != '\n') {
        }
    }

    // Returns the path of this program, or std::nullopt if the system does not report it.
    std::optional<fs::path> programPath() {
#ifdef _WIN32
        std::wstring module(MAX_PATH, L'\0');
        for (;;) {
            const auto length = GetModuleFileNameW(nullptr, module.data(), DWORD(module.size()));
            if (length == 0) {
                return std::nullopt;
            }
            if (length < module.size()) {
                module.resize(length);
                return fs::path(module);
            }
            module.resize(module.size() * 2);
        }
#elif defined(__APPLE__)
        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        std::string buffer(size, '\0');
        if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
            return std::nullopt;
        }
        return fs::path(buffer.c_str());
#else
        std::error_code ec;
        auto path = fs::read_symlink("/proc/self/exe", ec);
        if (ec) {
            return std::nullopt;
        }
        return path;
#endif
    }

#ifdef _WIN32
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
#endif

    // Runs the original program beside this program with the arguments that the plugin passed,
    // and waits for it. On Windows, the original receives the command line as written rather than
    // arguments, which are given for the other systems. Returns the exit status of the original,
    // or std::nullopt if the original is absent or cannot start.
    std::optional<int> runOriginal(const std::vector<NativeString> &arguments) {
        const auto program = programPath();
        if (!program) {
            return std::nullopt;
        }
        const auto original = program->parent_path() / originalName;
        std::error_code ec;
        if (!fs::is_regular_file(original, ec)) {
            return std::nullopt;
        }
#ifdef _WIN32
        static_cast<void>(arguments);
        // A file name cannot contain a quotation mark, which therefore needs no escape here.
        return kit::NativeProcess::runCommandLine(original, L"\"" + original.native() + L"\"" +
                                                                argumentsAsWritten());
#else
        return kit::NativeProcess::run(original, arguments);
#endif
    }

    // Shows text, which is ASCII, on the standard output. On Windows the text appears in a
    // message box instead, unless the standard output is redirected to a file or a pipe, because
    // a GUI program has no console of its own.
    void show(const char *text) {
#ifdef _WIN32
        const auto output = GetStdHandle(STD_OUTPUT_HANDLE);
        const auto type =
            output && output != INVALID_HANDLE_VALUE ? GetFileType(output) : FILE_TYPE_UNKNOWN;
        const std::string bytes(text);
        if (type == FILE_TYPE_DISK || type == FILE_TYPE_PIPE) {
            DWORD written = 0;
            WriteFile(output, bytes.data(), DWORD(bytes.size()), &written, nullptr);
            return;
        }
        kit::NativeMessageBox::show("hello-plugin-play", text, kit::NativeMessageBox::Information);
#else
        std::fputs(text, stdout);
        std::fflush(stdout);
#endif
    }

    // Shows the error text in a message box, or on the standard error if no message box can be
    // shown, because the plugin usually discards the output of this program.
    void showError(const char *text) {
        if (kit::NativeMessageBox::show("playinplugin", text, kit::NativeMessageBox::Critical) ==
            kit::NativeMessageBox::NoButton) {
            std::fputs(text, stderr);
            std::fputc('\n', stderr);
        }
    }

}

// The arguments are in UTF-8 on Windows, from HelloKitWinMain, and the bytes of the command line
// elsewhere.
int main(int argc, char *argv[]) {
    std::vector<NativeString> arguments;
    for (int i = 1; i < argc; ++i) {
        arguments.push_back(kit::nativeOf(argv[i]));
    }
    // The original exits at once without arguments.
    if (arguments.empty()) {
        return Success;
    }
    // The help and the version only as the only argument, so that the first argument of a
    // longer command line is the input file as in the original. A file name cannot contain the
    // question mark of /?.
    if (arguments.size() == 1) {
        const auto &argument = arguments.front();
        if (argument == HELLOKIT_NATIVE("/?") || argument == HELLOKIT_NATIVE("-h") ||
            argument == HELLOKIT_NATIVE("--help")) {
            show(usage);
            return Success;
        }
        if (argument == HELLOKIT_NATIVE("--version")) {
            show(version);
            return Success;
        }
    }
    if (const auto connection = connectToHost()) {
#ifdef _WIN32
        // This program was started by the plugin, the foreground process, and may therefore
        // pass the right to set the foreground window to the host, so that the preview dialog
        // of the host appears in front instead of flashing in the taskbar.
        ULONG host = 0;
        if (GetNamedPipeServerProcessId(*connection, &host)) {
            AllowSetForegroundWindow(host);
        }
#endif
        sendRequest(*connection, message(parse(arguments)));
        disconnect(*connection);
        return Success;
    }
    if (const auto code = runOriginal(arguments)) {
        return *code;
    }
    showError(noHost);
    return NoHost;
}
