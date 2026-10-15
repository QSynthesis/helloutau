#ifndef HELLOUTAU_WIDGETS_SCENEGESTURES_H
#define HELLOUTAU_WIDGETS_SCENEGESTURES_H

#include <functional>
#include <memory>

#include <QtCore/QPointF>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>
#include <helloutau/Widgets/SceneGesture.h>

namespace hello::daw {

    /// A press that becomes a click or a drag. A move farther than
    /// QApplication::startDragDistance() from the press starts the drag: the drag function
    /// receives the modifiers held then and returns the gesture of the drag, which receives
    /// that move, the later moves and the release. A release before that is a click: the click
    /// function receives the modifiers held then. The drag function may return no gesture, and
    /// the click function may do nothing. Without a click function, the first move starts the
    /// drag, and a release before any move ends a drag that does not move.
    class HELLOUTAU_WIDGETS_EXPORT PressGesture : public SceneGesture {
    public:
        using DragFunction = std::function<std::unique_ptr<SceneGesture>(Qt::KeyboardModifiers)>;
        using ClickFunction = std::function<void(Qt::KeyboardModifiers)>;

        PressGesture(QPointF position, DragFunction drag, ClickFunction click);
        ~PressGesture();

        void move(QPointF position, Qt::KeyboardModifiers modifiers) override;
        void release(QPointF position, Qt::KeyboardModifiers modifiers) override;
        void cancel() override;
        bool hasAutoScroll() const override;

    private:
        QPointF m_origin;
        DragFunction m_drag;
        ClickFunction m_click;
        bool m_started = false;
        // The gesture of the drag, if the drag started and the drag function returned a gesture
        std::unique_ptr<SceneGesture> m_gesture;
    };

}

#endif // HELLOUTAU_WIDGETS_SCENEGESTURES_H
