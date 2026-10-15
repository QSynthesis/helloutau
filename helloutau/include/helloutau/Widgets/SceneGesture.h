#ifndef HELLOUTAU_WIDGETS_SCENEGESTURE_H
#define HELLOUTAU_WIDGETS_SCENEGESTURE_H

#include <QtCore/QPointF>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

namespace hello::daw {

    /// One interaction from a press of a mouse button to its release, created by a layer for the
    /// element that was pressed.
    ///
    /// While the interaction lasts it keeps its own preview state, which its layer draws instead
    /// of the document. Only release() changes the document, in one step. cancel() leaves the
    /// document as it was. See the section on the piano roll in docs/Widgets.md.
    class HELLOUTAU_WIDGETS_EXPORT SceneGesture {
    public:
        virtual ~SceneGesture();

        /// The pointer moved to \a position, in view coordinates.
        virtual void move(QPointF position, Qt::KeyboardModifiers modifiers) = 0;

        /// The button was released at \a position: the interaction completes.
        virtual void release(QPointF position, Qt::KeyboardModifiers modifiers) = 0;

        /// The interaction was abandoned, on Escape or when the view lost the pointer.
        virtual void cancel() = 0;

        /// Returns whether the view scrolls while the pointer is near an edge: across near the
        /// left and right edges, up and down near the top and bottom edges.
        virtual bool hasAutoScroll() const {
            return false;
        }
    };

}

#endif // HELLOUTAU_WIDGETS_SCENEGESTURE_H
