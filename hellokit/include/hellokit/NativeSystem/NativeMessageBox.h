#ifndef HELLOKIT_NATIVESYSTEM_NATIVEMESSAGEBOX_H
#define HELLOKIT_NATIVESYSTEM_NATIVEMESSAGEBOX_H

#include <string>

namespace hello::kit {

    /// A message box of the system with an OK button, for the programs that link no GUI
    /// library. See NativeString.h for the library.
    class NativeMessageBox {
    public:
        enum Icon {
            NoIcon,      ///< no icon
            Information, ///< an information icon
            Question,    ///< a question icon
            Warning,     ///< a warning icon
            Critical,    ///< an error icon
        };

        /// Shows \a text with the window title \a title and \a icon , and waits until the user
        /// closes the box. Both strings are in UTF-8.
        ///
        /// The box is that of MessageBoxW() on Windows, with the icon of the same name. On macOS
        /// it is that of CFUserNotificationDisplayAlert(), with the alert level Plain, Note,
        /// Note, Caution, or Stop for the icons in order. On Linux it is shown by zenity or,
        /// without zenity, by kdialog, if either is installed and a display is available. Both
        /// show the information icon for \c NoIcon and \c Question , because their message
        /// boxes with an OK button have no other.
        ///
        /// \return whether the box was shown, so that the caller can write \a text elsewhere
        static bool show(const std::string &title, const std::string &text, Icon icon);
    };

}

#endif // HELLOKIT_NATIVESYSTEM_NATIVEMESSAGEBOX_H
