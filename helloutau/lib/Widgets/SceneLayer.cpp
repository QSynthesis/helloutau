#include "SceneLayer.h"

#include "SceneView.h"

namespace hello::daw {

    SceneGesture::~SceneGesture() = default;

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

    void SceneLayer::update() {
        if (m_view) {
            m_view->viewport()->update();
        }
    }

}
