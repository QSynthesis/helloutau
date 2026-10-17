#include "NativeMessageBox.h"

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>

#include <commctrl.h>

#include <algorithm>

#include "NativeString.h"

// TaskDialogIndirect() exists only in version 6 of the Common Controls, which a program obtains
// through the dependency in its manifest. The function is therefore looked up at run time rather
// than imported, because a program without that dependency would not start.

namespace hello::kit {

    namespace {

        using TaskDialogIndirectFunction = HRESULT(WINAPI *)(const TASKDIALOGCONFIG *, int *, int *,
                                                             BOOL *);

        // The identifier of a button in the task dialog, apart from IDOK, IDCANCEL and the other
        // identifiers of the common buttons
        constexpr int firstButtonId = 1000;

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

        PCWSTR taskIconOf(NativeMessageBox::Icon icon) {
            switch (icon) {
                case NativeMessageBox::Information:
                case NativeMessageBox::Question:
                    // The task dialog has no question icon.
                    return TD_INFORMATION_ICON;
                case NativeMessageBox::Warning:
                    return TD_WARNING_ICON;
                case NativeMessageBox::Critical:
                    return TD_ERROR_ICON;
                case NativeMessageBox::NoIcon:
                    break;
            }
            return nullptr;
        }

        TaskDialogIndirectFunction taskDialogIndirect() {
            // The module is that of the activation context of the program, which is version 6
            // if the manifest requires it.
            const auto module = ::LoadLibraryW(L"comctl32.dll");
            if (!module) {
                return nullptr;
            }
            return reinterpret_cast<TaskDialogIndirectFunction>(
                ::GetProcAddress(module, "TaskDialogIndirect"));
        }

        // Shows the box with MessageBoxW(), whose buttons are the combination of the same
        // number of buttons, OK and Cancel for two, or Yes, No and Cancel for three, with the
        // labels of the system. Cancel stands for the escape button, which Escape chooses, and
        // the other buttons stand for the rest in order.
        NativeMessageBox::StandardButton showMessageBox(const std::wstring &title,
                                                        const std::wstring &text,
                                                        NativeMessageBox::Icon icon,
                                                        const NativeMessageBox::Layout &layout) {
            static constexpr UINT combinations[] = {MB_OK, MB_OKCANCEL, MB_YESNOCANCEL};
            static constexpr UINT defaults[] = {MB_DEFBUTTON1, MB_DEFBUTTON2, MB_DEFBUTTON3};
            static constexpr int results[][3] = {
                {IDOK,  0,        0       },
                {IDOK,  IDCANCEL, 0       },
                {IDYES, IDNO,     IDCANCEL},
            };
            std::vector<NativeMessageBox::StandardButton> buttons;
            for (const auto button : layout.buttons) {
                if (button != layout.escapeButton) {
                    buttons.push_back(button);
                }
            }
            buttons.push_back(layout.escapeButton);

            const auto count = buttons.size() - 1;
            const auto defaultIndex =
                std::find(buttons.cbegin(), buttons.cend(), layout.defaultButton) -
                buttons.cbegin();
            const int result =
                ::MessageBoxW(nullptr, text.c_str(), title.c_str(),
                              combinations[count] | defaults[defaultIndex] | styleOf(icon));
            if (result == 0) {
                return NativeMessageBox::NoButton;
            }
            for (std::size_t i = 0; i < buttons.size(); ++i) {
                if (results[count][i] == result) {
                    return buttons[i];
                }
            }
            return layout.escapeButton;
        }

    }

    NativeMessageBox::StandardButton
        NativeMessageBox::show(const std::string &title, const std::string &text, Icon icon,
                               int buttons, StandardButton defaultButton,
                               const std::map<StandardButton, std::string> &texts) {
        const auto layout = layoutOf(buttons, defaultButton);
        const auto nativeTitle = nativeOf(title);
        const auto nativeText = nativeOf(text);

        const auto lonelyOk = layout.buttons.size() == 1 && layout.buttons.front() == Ok &&
                              texts.find(Ok) == texts.cend();
        const auto function = lonelyOk ? nullptr : taskDialogIndirect();
        if (!function) {
            return showMessageBox(nativeTitle, nativeText, icon, layout);
        }

        std::vector<std::wstring> labels;
        for (const auto button : layout.buttons) {
            const auto found = texts.find(button);
            labels.push_back(nativeOf(found != texts.cend() ? found->second : nameOf(button)));
        }
        std::vector<TASKDIALOG_BUTTON> taskButtons;
        int defaultId = firstButtonId;
        for (std::size_t i = 0; i < layout.buttons.size(); ++i) {
            const int id = firstButtonId + int(i);
            taskButtons.push_back({id, labels[i].c_str()});
            if (layout.buttons[i] == layout.defaultButton) {
                defaultId = id;
            }
        }

        TASKDIALOGCONFIG config{};
        config.cbSize = sizeof(config);
        // Escape and the close button of the title bar choose the escape button.
        config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
        config.pszWindowTitle = nativeTitle.c_str();
        config.pszMainIcon = taskIconOf(icon);
        config.pszContent = nativeText.c_str();
        config.cButtons = UINT(taskButtons.size());
        config.pButtons = taskButtons.data();
        config.nDefaultButton = defaultId;

        int pressed = 0;
        if (FAILED(function(&config, &pressed, nullptr, nullptr))) {
            return showMessageBox(nativeTitle, nativeText, icon, layout);
        }
        const auto index = std::size_t(pressed - firstButtonId);
        return index < layout.buttons.size() ? layout.buttons[index] : layout.escapeButton;
    }

}
