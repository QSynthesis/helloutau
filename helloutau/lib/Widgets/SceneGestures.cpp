#include "SceneGestures.h"

#include <QtWidgets/QApplication>

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

}
