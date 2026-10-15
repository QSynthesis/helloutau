#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

// The entry point of a program of the Windows subsystem whose source defines main, after
// qtentrypoint_win.cpp of Qt 6. The program receives the arguments in UTF-8, the encoding of the
// strings of this repository, rather than in the ANSI code page as Qt passes them. An unpaired
// surrogate in an argument becomes U+FFFD.
//
// The source calls only the Windows API, neither the C runtime nor the standard library, because
// the library serves programs of either runtime, see CMakeLists.txt. The arguments are split by
// CommandLineToArgvW, as in Qt.

// The main of the program. Its name is not mangled, as the name of main never is.
int main(int argc, char *argv[]);

namespace {

    // Returns \a text in UTF-8 on the process heap, or null if the heap is exhausted.
    char *utf8Of(const wchar_t *text) {
        const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
        const auto result =
            static_cast<char *>(HeapAlloc(GetProcessHeap(), 0, SIZE_T(size > 0 ? size : 1)));
        if (result && (size <= 0 || WideCharToMultiByte(CP_UTF8, 0, text, -1, result, size, nullptr,
                                                        nullptr) <= 0)) {
            result[0] = '\0';
        }
        return result;
    }

    void freeArguments(char **argv, int argc) {
        const auto heap = GetProcessHeap();
        for (int i = 0; i < argc; ++i) {
            if (argv[i]) {
                HeapFree(heap, 0, argv[i]);
            }
        }
        HeapFree(heap, 0, argv);
    }

    // Calls main with \a wideArguments in UTF-8. Returns -1 without calling main if the heap is
    // exhausted.
    int callMain(int argc, wchar_t *wideArguments[]) {
        const auto argv = static_cast<char **>(
            HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(char *) * SIZE_T(argc + 1)));
        if (!argv) {
            return -1;
        }
        for (int i = 0; i < argc; ++i) {
            argv[i] = utf8Of(wideArguments[i]);
            if (!argv[i]) {
                freeArguments(argv, argc);
                return -1;
            }
        }
        const int code = main(argc, argv);
        freeArguments(argv, argc);
        return code;
    }

}

#ifdef __MINGW32__
// MinGW-w64 starts wmain with -municode, which the library adds to the program.
extern "C" int wmain(int argc, wchar_t *argv[]) {
    return callMain(argc, argv);
}
#else
// The linker selects WinMainCRTStartup for a program of the Windows subsystem that defines
// WinMain, also if a library defines it.
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    int argc = 0;
    const auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        return -1;
    }
    const int code = callMain(argc, argv);
    LocalFree(argv);
    return code;
}
#endif
