#ifndef HELLOUTAU_WIDGETS_TOOLBARPALETTE_H
#define HELLOUTAU_WIDGETS_TOOLBARPALETTE_H

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

class QToolBar;

namespace hello::daw {

    /// Gives each button of \a toolBar white text for its checked state. The system accent color
    /// remains the checked background. The palette follows the color scheme of the
    /// application. Menus keep the palette of the application.
    HELLOUTAU_WIDGETS_EXPORT void followToolBarPalette(QToolBar *toolBar);

}

#endif // HELLOUTAU_WIDGETS_TOOLBARPALETTE_H
