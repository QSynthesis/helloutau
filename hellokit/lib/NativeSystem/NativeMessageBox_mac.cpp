#include "NativeMessageBox.h"

#include <CoreFoundation/CoreFoundation.h>

// CFUserNotification requires neither AppKit nor an application bundle, and therefore serves a
// program without a run loop of its own.

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

    }

    bool NativeMessageBox::show(const std::string &title, const std::string &text, Icon icon) {
        const auto header = stringOf(title);
        const auto message = stringOf(text);
        bool shown = false;
        if (header && message) {
            CFOptionFlags response = 0;
            // No timeout, and the default button, which is OK
            shown =
                CFUserNotificationDisplayAlert(0, levelOf(icon), nullptr, nullptr, nullptr, header,
                                               message, nullptr, nullptr, nullptr, &response) == 0;
        }
        if (header) {
            CFRelease(header);
        }
        if (message) {
            CFRelease(message);
        }
        return shown;
    }

}
