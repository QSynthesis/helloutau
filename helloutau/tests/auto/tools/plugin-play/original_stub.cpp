#include <fstream>
#include <iterator>
#include <string>

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>

// A stand-in for the original playinplugin.exe in test_plugin_play.py. Appends the command line
// and the working directory in UTF-8 to the file that the environment variable
// PLUGIN_PLAY_STUB_LOG names, one per line, and exits with the status 7.

namespace {

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

}

int wmain() {
    wchar_t log[MAX_PATH * 4];
    if (GetEnvironmentVariableW(L"PLUGIN_PLAY_STUB_LOG", log, DWORD(std::size(log))) != 0) {
        wchar_t directory[MAX_PATH * 4];
        const auto length = GetCurrentDirectoryW(DWORD(std::size(directory)), directory);
        std::ofstream out(log, std::ios::binary | std::ios::app);
        out << utf8(GetCommandLineW()) << '\n' << utf8(std::wstring(directory, length)) << '\n';
    }
    return 7;
}
