#include "NativeMessageBox.h"

#include <CoreFoundation/CoreFoundation.h>

#include <algorithm>

// CFUserNotification requires neither AppKit nor an application bundle, and therefore serves a
// program without a run loop of its own. The alert has a default button at the right, an
// alternate button left of it and an other button at the left. The documentation specifies no
// response to Escape, so the escape button takes the alternate button, and the cancel response
// chooses it as well.

namespace hello::kit {

    namespace {

        CFOptionFlags levelOf(NativeMessageBox::Icon icon) {
            switch (icon) {
                case NativeMessageBox::Information:
                case NativeMessageBox::Question:
                    return kCFUserNotificationNoteAlertLevel;
                case NativeMessageBox::Warning:
                    return kCFUserNotificationCautionAlertLevel;
                case NativeMessageBox::Critical:
                    return kCFUserNotificationStopAlertLevel;
                case NativeMessageBox::NoIcon:
                    break;
            }
            return kCFUserNotificationPlainAlertLevel;
        }

        // Returns text as a string, or null if it is no valid UTF-8. The caller releases the
        // string.
        CFStringRef stringOf(const std::string &text) {
            return CFStringCreateWithBytes(kCFAllocatorDefault,
                                           reinterpret_cast<const UInt8 *>(text.data()),
                                           CFIndex(text.size()), kCFStringEncodingUTF8, false);
        }

        // The strings of a box, released on destruction
        struct Strings {
            std::vector<CFStringRef> items;

            ~Strings() {
                for (const auto item : items) {
                    if (item) {
                        CFRelease(item);
                    }
                }
            }

            CFStringRef add(const std::string &text) {
                items.push_back(stringOf(text));
                return items.back();
            }
        };

    }

    NativeMessageBox::StandardButton
        NativeMessageBox::show(const std::string &title, const std::string &text, Icon icon,
                               int buttons, StandardButton defaultButton,
                               const std::map<StandardButton, std::string> &texts) {
        const auto layout = layoutOf(buttons, defaultButton);

        // The default button in the slot of the default response, the escape button in that of
        // the alternate response, and the other button, if any, in that of the other response
        std::vector<StandardButton> slots{layout.defaultButton};
        if (layout.escapeButton != layout.defaultButton) {
            slots.push_back(layout.escapeButton);
        }
        for (const auto button : layout.buttons) {
            if (std::find(slots.cbegin(), slots.cend(), button) == slots.cend()) {
                slots.push_back(button);
            }
        }

        Strings strings;
        const auto header = strings.add(title);
        const auto message = strings.add(text);
        CFStringRef titles[3] = {};
        for (std::size_t i = 0; i < slots.size(); ++i) {
            const auto found = texts.find(slots[i]);
            if (found != texts.cend()) {
                titles[i] = strings.add(found->second);
            } else if (slots.size() > 1 || slots[i] != Ok) {
                titles[i] = strings.add(nameOf(slots[i]));
            }
            // A single Ok without a text has the title of the system.
        }
        if (!header || !message) {
            return NoButton;
        }

        CFOptionFlags response = 0;
        // No timeout
        if (CFUserNotificationDisplayAlert(0, levelOf(icon), nullptr, nullptr, nullptr, header,
                                           message, titles[0], titles[1], titles[2],
                                           &response) != 0) {
            return NoButton;
        }
        switch (response & 0x3) {
            case kCFUserNotificationDefaultResponse:
                return slots[0];
            case kCFUserNotificationAlternateResponse:
                if (slots.size() > 1) {
                    return slots[1];
                }
                break;
            case kCFUserNotificationOtherResponse:
                if (slots.size() > 2) {
                    return slots[2];
                }
                break;
            default:
                break;
        }
        return layout.escapeButton;
    }

}
