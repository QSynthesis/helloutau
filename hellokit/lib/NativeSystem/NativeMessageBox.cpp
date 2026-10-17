#include "NativeMessageBox.h"

#include <algorithm>

namespace hello::kit {

    NativeMessageBox::Layout NativeMessageBox::layoutOf(int buttons, StandardButton defaultButton) {
        Layout layout;
        for (const auto button : {Ok, Cancel, Yes, No, Continue, Quit}) {
            if ((buttons & button) && layout.buttons.size() < 3) {
                layout.buttons.push_back(button);
            }
        }
        if (layout.buttons.empty()) {
            layout.buttons.push_back(Ok);
        }
        const auto contains = [&layout](StandardButton button) {
            return std::find(layout.buttons.cbegin(), layout.buttons.cend(), button) !=
                   layout.buttons.cend();
        };
        layout.defaultButton = contains(defaultButton) ? defaultButton : layout.buttons.front();
        layout.escapeButton = layout.buttons.back();
        for (const auto button : {Cancel, No, Quit}) {
            if (contains(button)) {
                layout.escapeButton = button;
                break;
            }
        }
        return layout;
    }

    std::string NativeMessageBox::nameOf(StandardButton button) {
        switch (button) {
            case Ok:
                return "OK";
            case Cancel:
                return "Cancel";
            case Yes:
                return "Yes";
            case No:
                return "No";
            case Continue:
                return "Continue";
            case Quit:
                return "Quit";
            case NoButton:
                break;
        }
        return {};
    }

}
