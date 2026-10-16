#include "NativeMessageBox.h"

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>

#include "NativeString.h"

namespace hello::kit {

    namespace {

        UINT styleOf(NativeMessageBox::Icon icon) {
            switch (icon) {
                case NativeMessageBox::Information:
                    return MB_ICONINFORMATION;
                case NativeMessageBox::Question:
                    return MB_ICONQUESTION;
                case NativeMessageBox::Warning:
                    return MB_ICONWARNING;
                case NativeMessageBox::Critical:
                    return MB_ICONERROR;
                case NativeMessageBox::NoIcon:
                    break;
            }
            return 0;
        }

    }

    bool NativeMessageBox::show(const std::string &title, const std::string &text, Icon icon) {
        return MessageBoxW(nullptr, nativeOf(text).c_str(), nativeOf(title).c_str(),
                           MB_OK | styleOf(icon)) != 0;
    }

}
