#ifndef HELLOUTAU_WIDGETS_PLATFORMWHEEL_H
#define HELLOUTAU_WIDGETS_PLATFORMWHEEL_H

#include <QtCore/QPoint>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

class QWheelEvent;

namespace hello::daw {

    /// The wheel input as the device reported it, before the platform plugin altered it.
    class HELLOUTAU_WIDGETS_EXPORT PlatformWheel {
    public:
        /// Returns the angle delta of \a event, transposed back if the platform plugin
        /// transposed it because Alt is held. The plugins \c windows
        /// (qwindowspointerhandler.cpp) and \c xcb (qxcbwindow.cpp) of Qt 6.11 do so. A
        /// horizontal wheel with Alt held is therefore reported as vertical on those platforms,
        /// because the event no longer distinguishes the two.
        static QPoint angleDelta(const QWheelEvent &event);
    };

}

#endif // HELLOUTAU_WIDGETS_PLATFORMWHEEL_H
