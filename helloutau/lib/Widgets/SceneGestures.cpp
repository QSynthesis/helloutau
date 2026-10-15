#include "SceneGestures.h"

#include <cmath>

#include <QtWidgets/QApplication>

#include "SceneView.h"

namespace hello::daw {

    PressGesture::PressGesture(QPointF position, DragFunction drag, ClickFunction click)
        : m_origin(position), m_drag(std::move(drag)), m_click(std::move(click)) {
    }

    PressGesture::~PressGesture() = default;

    void PressGesture::move(QPointF position, Qt::KeyboardModifiers modifiers) {
        if (!m_started) {
            // Without a click, any move is a drag.
            if (m_click &&
                (position - m_origin).manhattanLength() < QApplication::startDragDistance()) {
                return;
            }
            m_started = true;
            if (m_drag) {
                m_gesture = m_drag(modifiers);
            }
        }
        if (m_gesture) {
            m_gesture->move(position, modifiers);
        }
    }

    void PressGesture::release(QPointF position, Qt::KeyboardModifiers modifiers) {
        if (!m_started && m_click) {
            m_click(modifiers);
            return;
        }
        // Without a click, a release before any move ends a drag that does not move.
        if (!m_started) {
            m_started = true;
            if (m_drag) {
                m_gesture = m_drag(modifiers);
            }
        }
        if (m_gesture) {
            m_gesture->release(position, modifiers);
        }
    }

    void PressGesture::cancel() {
        if (m_gesture) {
            m_gesture->cancel();
        }
    }

    bool PressGesture::hasAutoScroll() const {
        return m_gesture && m_gesture->hasAutoScroll();
    }

    SceneZoomGesture::SceneZoomGesture(SceneView *view, QPointF position, LockFunction locked)
        : m_view(view), m_origin(position), m_last(position), m_locked(std::move(locked)) {
    }

    SceneZoomGesture::~SceneZoomGesture() = default;

    void SceneZoomGesture::move(QPointF position, Qt::KeyboardModifiers modifiers) {
        const bool locked = m_locked && m_locked(modifiers);
        if (!locked) {
            m_lockOrigin.reset();
            m_orientation.reset();
        } else if (!m_lockOrigin) {
            m_lockOrigin = m_last;
        }
        if (locked && !m_orientation) {
            const QPointF distance = position - *m_lockOrigin;
            if (distance.manhattanLength() < QApplication::startDragDistance()) {
                return;
            }
            m_orientation =
                std::abs(distance.x()) >= std::abs(distance.y()) ? Qt::Horizontal : Qt::Vertical;
        }
        const double horizontal = position.x() - m_last.x();
        const double vertical = position.y() - m_last.y();
        m_last = position;
        if (locked) {
            const double delta = *m_orientation == Qt::Horizontal ? horizontal : vertical;
            if (delta == 0) {
                return;
            }
            const double factor = std::pow(1.01, delta);
            if (*m_orientation == Qt::Horizontal) {
                m_view->zoomTime(factor, m_origin.x());
            } else {
                m_view->zoomKeys(factor, m_origin.y());
            }
        } else {
            if (horizontal != 0) {
                m_view->zoomTime(std::pow(1.01, horizontal), m_origin.x());
            }
            if (vertical != 0) {
                m_view->zoomKeys(std::pow(1.01, vertical), m_origin.y());
            }
        }
    }

    void SceneZoomGesture::release(QPointF position, Qt::KeyboardModifiers modifiers) {
        move(position, modifiers);
    }

    void SceneZoomGesture::cancel() {
    }

}
