#ifndef HELLOUTAU_WIDGETS_MOUSEGRABGUARD_H
#define HELLOUTAU_WIDGETS_MOUSEGRABGUARD_H

#include <memory>

#include <QtCore/QEvent>
#include <QtCore/QList>
#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

namespace hello::daw {

    /// Grabs the mouse for a widget during a drag, and filters the events of the whole
    /// application meanwhile. A drag that leaves the widget then keeps its events, and the events
    /// that the drag causes reach no other widget. For example, a drag with the right button
    /// suppresses QEvent::ContextMenu, which would otherwise open a menu wherever it is
    /// delivered, such as the menu of the tool bars of the window.
    ///
    /// The destructor releases the grab if the widget still holds it. The filter remains until
    /// the events already posted when the grab ends are delivered, because some of them follow
    /// the release: on Windows, the context menu event of the right button follows its release.
    ///
    /// \note A grab ends a grab of another widget. A caller that replaces a grab destroys the
    ///       previous one before creating the next, because the destruction of the previous one
    ///       would otherwise release the next.
    class HELLOUTAU_WIDGETS_EXPORT MouseGrabGuard {
    public:
        /// Grabs the mouse for \a widget and consumes every event of the application whose type
        /// is one of \a suppressedEvents.
        explicit MouseGrabGuard(QWidget *widget, const QList<QEvent::Type> &suppressedEvents = {});

        /// Grabs the mouse for \a widget and installs \a filter on the application. The grab
        /// owns \a filter, which may be null.
        MouseGrabGuard(QWidget *widget, std::unique_ptr<QObject> filter);

        ~MouseGrabGuard();

        QWidget *widget() const;

    private:
        QPointer<QWidget> m_widget;
        QPointer<QObject> m_filter;

        Q_DISABLE_COPY_MOVE(MouseGrabGuard)
    };

}

#endif // HELLOUTAU_WIDGETS_MOUSEGRABGUARD_H
