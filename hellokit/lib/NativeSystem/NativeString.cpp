#include "NativeString.h"

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace hello::kit {

    std::string utf8Of(const NativeString &text) {
#ifdef _WIN32
        if (text.empty()) {
            return {};
        }
        const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0,
                                             nullptr, nullptr);
        std::string out(static_cast<size_t>(size > 0 ? size : 0), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), out.data(), size, nullptr,
                            nullptr);
        return out;
#else
        return text;
#endif
    }

    NativeString nativeOf(const std::string &text) {
#ifdef _WIN32
        if (text.empty()) {
            return {};
        }
        const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0);
        std::wstring out(static_cast<size_t>(size > 0 ? size : 0), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), out.data(), size);
        return out;
#else
        return text;
#endif
    }

}
