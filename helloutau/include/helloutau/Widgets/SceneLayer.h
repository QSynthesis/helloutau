#ifndef HELLOUTAU_WIDGETS_SCENELAYER_H
#define HELLOUTAU_WIDGETS_SCENELAYER_H

#include <memory>
#include <optional>

#include <QtCore/QPointF>
#include <QtCore/QRect>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>
#include <helloutau/Widgets/SceneGesture.h>

class QPainter;

namespace hello::daw {

    class SceneView;

    /// The element that a position of a scene view shows, as the layer that drew it reports it.
    ///
    /// Elements are not objects: a layer identifies the drawn element by values, typically the
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

    /// One layer of a scene view, which draws the part of the scene that is visible and reports
    /// the element drawn at a position.
    ///
    /// The layers of a view are drawn in the order they were added, and queried for hits in the
    /// reverse order, so that the element drawn on top is hit first.
    class HELLOUTAU_WIDGETS_EXPORT SceneLayer {
    public:
        virtual ~SceneLayer();

        /// The view the layer belongs to, or \c nullptr before it is added to one.
        SceneView *view() const;

        /// Draws the layer. \a exposed is the part of the view to draw, in view coordinates.
        virtual void paint(QPainter &painter, const QRect &exposed) = 0;

        /// Returns the element that the layer drew at \a position, in view coordinates, or
        /// \c std::nullopt if there is no element that responds to the pointer.
        virtual std::optional<SceneHit> hitTest(QPointF position) const;

        /// Starts an interaction with the element that \a hit reports, or returns \c nullptr if a
        /// press on it does nothing.
        virtual std::unique_ptr<SceneGesture> press(const SceneHit &hit, QPointF position,
                                                    Qt::MouseButton button,
                                                    Qt::KeyboardModifiers modifiers);

        /// Responds to a double click on the element that \a hit reports, and returns whether it
        /// did.
        ///
        /// The press of the first click has already been delivered to press(). If the layer does
        /// not respond, the second press is delivered to press() as well.
        virtual bool doubleClick(const SceneHit &hit, QPointF position);

        /// Schedules the view to draw the layer again.
        void update();

    private:
        SceneView *m_view = nullptr;

        friend class SceneView;
    };

}

#endif // HELLOUTAU_WIDGETS_SCENELAYER_H
