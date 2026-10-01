#ifndef HELLOUTAU_EDITOR_TOOLBARPALETTE_P_H
#define HELLOUTAU_EDITOR_TOOLBARPALETTE_P_H

class QToolBar;

namespace hello::daw {

    /// Gives each button of \a toolBar white text for its checked state. The system accent color
    /// remains the checked background. The palette follows the color scheme of the
    /// application. Menus keep the palette of the application.
    void followToolBarPalette(QToolBar *toolBar);

}

#endif // HELLOUTAU_EDITOR_TOOLBARPALETTE_P_H
