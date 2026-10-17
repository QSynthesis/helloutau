#ifndef HELLOKIT_NATIVESYSTEM_NATIVEMESSAGEBOX_H
#define HELLOKIT_NATIVESYSTEM_NATIVEMESSAGEBOX_H

#include <map>
#include <string>
#include <vector>

namespace hello::kit {

    /// A message box of the system, for the programs that link no GUI library. See
    /// NativeString.h for the library.
    class NativeMessageBox {
    public:
        enum Icon {
            NoIcon,      ///< no icon
            Information, ///< an information icon
            Question,    ///< a question icon
            Warning,     ///< a warning icon
            Critical,    ///< an error icon
        };

        /// The buttons, as in QMessageBox, which can be combined with the bitwise OR. The box
        /// shows them from left to right in the order of the values.
        enum StandardButton {
            NoButton = 0x00, ///< no button
            Ok = 0x01,       ///< OK
            Cancel = 0x02,   ///< Cancel
            Yes = 0x04,      ///< Yes
            No = 0x08,       ///< No
            Continue = 0x10, ///< Continue
            Quit = 0x20,     ///< Quit
        };

        /// Shows \a text with the window title \a title, \a icon and \a buttons, of which at
        /// most three are supported, and waits until the user closes the box. The strings are
        /// in UTF-8.
        ///
        /// \a defaultButton is selected at first, and the first of \a buttons if it is
        /// \c NoButton. The escape button is \c Cancel, \c No or \c Quit, the first of them in
        /// \a buttons, or the last of \a buttons if none is included. Closing the box without a
        /// button chooses the escape button.
        ///
        /// The library has no translations, so the caller passes the text of a button in
        /// \a texts. A button without a text shows its English name. The single \c Ok button
        /// without a text shows the label of the system instead.
        ///
        /// The box is that of TaskDialogIndirect() on Windows, or that of MessageBoxW(), with
        /// the labels of the system, if the program does not use version 6 of the Common
        /// Controls. On macOS it is that of CFUserNotificationDisplayAlert(), with the alert
        /// level Plain, Note, Note, Caution, or Stop for the icons in order, and the default
        /// button at the right. On Linux it is shown by zenity or, without zenity, by kdialog,
        /// if either is installed and a display is available. zenity shows a question icon for
        /// more than one button, and selects the escape button at first if it is the default
        /// button, or the first other button otherwise. kdialog always selects the first button.
        ///
        /// \return the button that the user chose, or \c NoButton if the box was not shown, so
        ///         that the caller can write \a text elsewhere
        static StandardButton show(const std::string &title, const std::string &text, Icon icon,
                                   int buttons = Ok, StandardButton defaultButton = NoButton,
                                   const std::map<StandardButton, std::string> &texts = {});

        /// The arrangement of the buttons of a box, the same on every platform.
        struct Layout {
            /// The buttons from left to right, at least one and at most three.
            std::vector<StandardButton> buttons;
            StandardButton defaultButton = Ok; ///< the button selected at first
            StandardButton escapeButton = Ok;  ///< the button chosen by closing the box
        };

        /// Returns the arrangement that show() uses for \a buttons and \a defaultButton. Without
        /// any button in \a buttons, the box has the button \c Ok. A \a defaultButton that is
        /// not in the box is replaced by the first button.
        static Layout layoutOf(int buttons, StandardButton defaultButton);

        /// Returns the English name of \a button, which a button without a text shows.
        static std::string nameOf(StandardButton button);
    };

}

#endif // HELLOKIT_NATIVESYSTEM_NATIVEMESSAGEBOX_H
