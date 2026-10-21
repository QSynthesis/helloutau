#ifndef HELLOUTAU_WIDGETS_WINDOWPLACEMENT_H
#define HELLOUTAU_WIDGETS_WINDOWPLACEMENT_H

#include <QtCore/QList>
#include <QtCore/QPoint>
#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

class QScreen;

namespace hello::daw {

    /// The position of a new top-level window on the screen of the mouse pointer, rather than on
    /// the screen of the active window, or where the system puts a window, which is the primary
    /// screen on Windows (author's request, 2026-10-21). Wayland ignores the position.
    class HELLOUTAU_WIDGETS_EXPORT WindowPlacement {
    public:
        /// The offset of a cascaded window from its anchor, in both directions
        static constexpr int cascadeStep = 32;

        /// Returns the window of \a windows other than \a window from which a new window is
        /// cascaded: the window under \a pointer, or else the last visible window on \a screen,
        /// or \c nullptr if \a screen shows none of them.
        static QWidget *anchorOf(const QList<QPointer<QWidget>> &windows, const QWidget *window,
                                 const QPoint &pointer, const QScreen *screen);

        /// Moves \a window, a new top-level window that is not shown yet, onto the screen of
        /// \a pointer, or of the primary screen if no screen contains \a pointer: cascaded from
        /// the window that anchorOf() returns among \a windows, or else centered on the screen,
        /// and kept within the available area of the screen.
        static void place(QWidget *window, const QList<QPointer<QWidget>> &windows,
                          const QPoint &pointer);

    private:
        WindowPlacement() = delete;
    };

}

#endif // HELLOUTAU_WIDGETS_WINDOWPLACEMENT_H
