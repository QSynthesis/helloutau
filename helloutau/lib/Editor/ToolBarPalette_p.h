#ifndef HELLOUTAU_EDITOR_TOOLBARPALETTE_P_H
#define HELLOUTAU_EDITOR_TOOLBARPALETTE_P_H

class QToolBar;

namespace hello::daw {

    /// Gives each button of \a toolBar polished after the call a subtle background for the
    /// checked state in place of the accent color, as in the tool bars of JetBrains IDEs, and
    /// the window text color for its text and icon. The palette follows the color scheme of the
    /// application. Menus keep the palette of the application.
    void followToolBarPalette(QToolBar *toolBar);

}

#endif // HELLOUTAU_EDITOR_TOOLBARPALETTE_P_H
