#include "SceneLayer.h"

#include <QtWidgets/QApplication>

#include "SceneView.h"

namespace hello::daw {

    SceneGesture::~SceneGesture() = default;

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

    bool PressGesture::wantsAutoScroll() const {
        return m_gesture && m_gesture->wantsAutoScroll();
    }

    SceneLayer::~SceneLayer() = default;

    SceneView *SceneLayer::view() const {
        return m_view;
    }

    std::optional<SceneHit> SceneLayer::hitTest(QPointF position) const {
        Q_UNUSED(position);
        return std::nullopt;
    }

    std::unique_ptr<SceneGesture> SceneLayer::press(const SceneHit &hit, QPointF position,
                                                    Qt::MouseButton button,
                                                    Qt::KeyboardModifiers modifiers) {
        Q_UNUSED(hit);
        Q_UNUSED(position);
        Q_UNUSED(button);
        Q_UNUSED(modifiers);
        return nullptr;
    }

    bool SceneLayer::doubleClick(const SceneHit &hit, QPointF position) {
        Q_UNUSED(hit);
        Q_UNUSED(position);
        return false;
    }

    void SceneLayer::update() {
        if (m_view) {
            m_view->viewport()->update();
        }
    }

}
