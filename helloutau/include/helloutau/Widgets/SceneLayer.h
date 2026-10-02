#ifndef HELLOUTAU_WIDGETS_SCENELAYER_H
#define HELLOUTAU_WIDGETS_SCENELAYER_H

#include <memory>
#include <optional>

#include <QtCore/QPointF>
#include <QtCore/QRect>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

class QPainter;

namespace hello::daw {

    class SceneView;

    /// What a position of a scene view shows, as the layer that drew it reports it.
    ///
    /// Elements are not objects: a layer identifies what it drew by values, typically the
    /// identifier of a node of a document and a part of it, and finds the element again from
    /// them. \c node is therefore an opaque number to the view.
    struct SceneHit {
        /// The layer that reported the hit, set by the view.
        int layer = -1;

        quint64 node = 0;

        /// The part of the element, such as its body or its right edge, as the layer defines it.
        int part = 0;

        /// A position within the part, such as the index of a control point.
        int index = 0;

        Qt::CursorShape cursor = Qt::ArrowCursor;

        inline bool operator==(const SceneHit &RHS) const {
            return layer == RHS.layer && node == RHS.node && part == RHS.part &&
                   index == RHS.index && cursor == RHS.cursor;
        }
    };

    /// One interaction from a press of a mouse button to its release, created by a layer for
    /// what was pressed.
    ///
    /// While the interaction lasts it keeps its own preview state, which its layer draws instead
    /// of the document. Only release() changes the document, in one step; cancel() leaves the
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

        /// Whether the view should scroll horizontally while the pointer is near an edge.
        virtual bool wantsAutoScroll() const { return false; }
    };

    /// One layer of a scene view, which draws the part of the scene that is visible and reports
    /// what it drew at a position.
    ///
    /// The layers of a view are drawn in the order they were added, and asked for hits in the
    /// reverse order, so that what is drawn on top is hit first.
    class HELLOUTAU_WIDGETS_EXPORT SceneLayer {
    public:
        virtual ~SceneLayer();

        /// The view the layer belongs to, or \c nullptr before it is added to one.
        SceneView *view() const;

        /// Draws the layer. \a exposed is the part of the view to draw, in view coordinates.
        virtual void paint(QPainter &painter, const QRect &exposed) = 0;

        /// Returns what the layer drew at \a position, in view coordinates, or \c std::nullopt
        /// if nothing that responds to the pointer.
        virtual std::optional<SceneHit> hitTest(QPointF position) const;

        /// Starts an interaction with what \a hit reports, or returns \c nullptr if a press on it
        /// does nothing.
        virtual std::unique_ptr<SceneGesture> press(const SceneHit &hit, QPointF position,
                                                    Qt::MouseButton button,
                                                    Qt::KeyboardModifiers modifiers);

        /// Responds to a double click on what \a hit reports, and returns whether it did.
        ///
        /// The press of the first click has already been delivered to press(). If the layer does
        /// not respond, the second press is delivered to press() as well.
        virtual bool doubleClick(const SceneHit &hit, QPointF position);

        /// Asks the view to draw the layer again.
        void update();

    private:
        SceneView *m_view = nullptr;

        friend class SceneView;
    };

}

#endif // HELLOUTAU_WIDGETS_SCENELAYER_H
