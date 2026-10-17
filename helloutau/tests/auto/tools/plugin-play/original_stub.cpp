#include <fstream>
#include <string>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>

#  include <iterator>
#else
#  include <cstdlib>
#  include <filesystem>
#  include <system_error>
#endif

#include <hellokit/NativeSystem/NativeString.h>

// A stand-in for the original playinplugin in test_plugin_play.py. Appends the command line and
// the working directory in UTF-8 to the file that the environment variable PLUGIN_PLAY_STUB_LOG
// names, one per line, and exits with the status 7. The first line is the command line of the
// process on Windows, and the arguments separated by tabs elsewhere.

namespace kit = hello::kit;

#ifdef _WIN32
int wmain() {
    wchar_t log[MAX_PATH * 4];
    if (GetEnvironmentVariableW(L"PLUGIN_PLAY_STUB_LOG", log, DWORD(std::size(log))) != 0) {
        wchar_t directory[MAX_PATH * 4];
        const auto length = GetCurrentDirectoryW(DWORD(std::size(directory)), directory);
        std::ofstream out(log, std::ios::binary | std::ios::app);
        out << kit::utf8Of(GetCommandLineW()) << '\n'
            << kit::utf8Of(std::wstring(directory, length)) << '\n';
    }
    return 7;
}
#else
int main(int argc, char *argv[]) {
    if (const auto log = std::getenv("PLUGIN_PLAY_STUB_LOG")) {
        std::string line;
        for (int i = 0; i < argc; ++i) {
            if (i > 0) {
                line += '\t';
            }
            line += argv[i];
        }
        std::error_code ec;
        std::ofstream out(log, std::ios::binary | std::ios::app);
        out << line << '\n' << kit::utf8Of(std::filesystem::current_path(ec).native()) << '\n';
    }
    return 7;
}
#endif
