#include "SceneView.h"

#include <algorithm>
#include <cmath>

#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QScrollBar>

namespace hello::daw {

    namespace {

        // One notch of a wheel is 120 units of angle, and zooms by this factor.
        constexpr double ZoomStep = 1.25;
        constexpr double NotchAngle = 120;

        // One notch scrolls this many rows up or down, or this part of the width across.
        constexpr double RowsPerNotch = 3;
        constexpr double WidthPerNotch = 0.125;

    }

    SceneView::SceneView(QWidget *parent) : QAbstractScrollArea(parent) {
        setFocusPolicy(Qt::StrongFocus);
        viewport()->setMouseTracking(true);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        updateScrollBars();
    }

    SceneView::~SceneView() = default;

    const TimeAxis &SceneView::timeAxis() const {
        return m_timeAxis;
    }

    void SceneView::setTimeAxis(const TimeAxis &axis) {
        const auto next = clamped(axis);
        if (next == m_timeAxis) {
            return;
        }
        m_timeAxis = next;
        updateScrollBars();
        viewport()->update();
        Q_EMIT timeAxisChanged();
    }

    const KeyAxis &SceneView::keyAxis() const {
        return m_keyAxis;
    }

    void SceneView::setKeyAxis(const KeyAxis &axis) {
        const auto next = clamped(axis);
        if (next == m_keyAxis) {
            return;
        }
        m_keyAxis = next;
        updateScrollBars();
        viewport()->update();
        Q_EMIT keyAxisChanged();
    }

    void SceneView::setTickRange(double first, double last) {
        m_firstTick = first;
        m_lastTick = std::max(first, last);
        setTimeAxis(m_timeAxis);
        updateScrollBars();
    }

    void SceneView::setKeyRange(int lowest, int highest) {
        m_lowestKey = lowest;
        m_highestKey = std::max(lowest, highest);
        setKeyAxis(m_keyAxis);
        updateScrollBars();
    }

    void SceneView::setTimeScaleRange(double minimum, double maximum) {
        m_minimumTimeScale = minimum;
        m_maximumTimeScale = std::max(minimum, maximum);
        setTimeAxis(m_timeAxis);
    }

    void SceneView::setKeyScaleRange(double minimum, double maximum) {
        m_minimumKeyScale = minimum;
        m_maximumKeyScale = std::max(minimum, maximum);
        setKeyAxis(m_keyAxis);
    }

    void SceneView::zoomTime(double factor, double anchor) {
        const double tick = m_timeAxis.toTick(anchor);
        TimeAxis axis = m_timeAxis;
        axis.pixelsPerTick =
            std::clamp(axis.pixelsPerTick * factor, m_minimumTimeScale, m_maximumTimeScale);
        axis.left = tick - anchor / axis.pixelsPerTick;
        setTimeAxis(axis);
    }

    void SceneView::zoomKeys(double factor, double anchor) {
        const double key = m_keyAxis.toKey(anchor);
        KeyAxis axis = m_keyAxis;
        axis.pixelsPerKey =
            std::clamp(axis.pixelsPerKey * factor, m_minimumKeyScale, m_maximumKeyScale);
        axis.top = key + anchor / axis.pixelsPerKey;
        setKeyAxis(axis);
    }

    SceneLayer *SceneView::addLayer(std::unique_ptr<SceneLayer> layer) {
        layer->m_view = this;
        m_layers.push_back(std::move(layer));
        viewport()->update();
        return m_layers.back().get();
    }

    int SceneView::layerCount() const {
        return int(m_layers.size());
    }

    SceneLayer *SceneView::layer(int index) const {
        return m_layers.at(size_t(index)).get();
    }

    std::optional<SceneHit> SceneView::hitAt(QPointF position) const {
        for (auto i = int(m_layers.size()) - 1; i >= 0; --i) {
            if (auto hit = m_layers[size_t(i)]->hitTest(position)) {
                hit->layer = i;
                return hit;
            }
        }
        return std::nullopt;
    }

    std::optional<SceneHit> SceneView::hoveredHit() const {
        return m_hovered;
    }

    bool SceneView::hasGesture() const {
        return m_gesture != nullptr;
    }

    void SceneView::paintEvent(QPaintEvent *event) {
        QPainter painter(viewport());
        for (const auto &layer : m_layers) {
            painter.save();
            layer->paint(painter, event->rect());
            painter.restore();
        }
    }

    void SceneView::resizeEvent(QResizeEvent *event) {
        QAbstractScrollArea::resizeEvent(event);
        // A larger view may show past the end of the scene, which moves the axes back.
        setTimeAxis(m_timeAxis);
        setKeyAxis(m_keyAxis);
        updateScrollBars();
    }

    void SceneView::scrollContentsBy(int dx, int dy) {
        Q_UNUSED(dx);
        Q_UNUSED(dy);
        if (m_updatingScrollBars) {
            return;
        }
        TimeAxis time = m_timeAxis;
        time.left = m_firstTick + horizontalScrollBar()->value() / time.pixelsPerTick;
        KeyAxis keys = m_keyAxis;
        keys.top = m_highestKey + 1 - verticalScrollBar()->value() / keys.pixelsPerKey;
        setTimeAxis(time);
        setKeyAxis(keys);
    }

    void SceneView::wheelEvent(QWheelEvent *event) {
        const auto delta = event->angleDelta();
        const auto modifiers = event->modifiers();
        const auto position = event->position();
        const double notches = (delta.y() != 0 ? delta.y() : delta.x()) / NotchAngle;

        if (modifiers & Qt::ControlModifier) {
            const double factor = std::pow(ZoomStep, notches);
            if (modifiers & Qt::ShiftModifier) {
                zoomKeys(factor, position.y());
            } else {
                zoomTime(factor, position.x());
            }
        } else if ((modifiers & Qt::ShiftModifier) || delta.y() == 0) {
            TimeAxis axis = m_timeAxis;
            axis.left -= notches * WidthPerNotch * viewport()->width() / axis.pixelsPerTick;
            setTimeAxis(axis);
        } else {
            KeyAxis axis = m_keyAxis;
            axis.top += notches * RowsPerNotch;
            setKeyAxis(axis);
        }
        updateHover(position);
        event->accept();
    }

    void SceneView::mousePressEvent(QMouseEvent *event) {
        setFocus(Qt::MouseFocusReason);
        cancelGesture();
        const auto position = event->position();
        if (const auto hit = hitAt(position)) {
            m_gesture = m_layers[size_t(hit->layer)]->press(*hit, position, event->button(),
                                                            event->modifiers());
        }
        event->accept();
    }

    void SceneView::mouseMoveEvent(QMouseEvent *event) {
        if (m_gesture) {
            m_gesture->move(event->position(), event->modifiers());
        } else {
            updateHover(event->position());
        }
        event->accept();
    }

    void SceneView::mouseReleaseEvent(QMouseEvent *event) {
        if (m_gesture) {
            // Released before release() runs, which may change what the view shows.
            auto gesture = std::move(m_gesture);
            gesture->release(event->position(), event->modifiers());
        }
        updateHover(event->position());
        event->accept();
    }

    void SceneView::mouseDoubleClickEvent(QMouseEvent *event) {
        const auto position = event->position();
        const auto hit = hitAt(position);
        if (event->button() == Qt::LeftButton && hit &&
            m_layers[size_t(hit->layer)]->doubleClick(*hit, position)) {
            cancelGesture();
            event->accept();
            return;
        }
        mousePressEvent(event);
    }

    void SceneView::keyPressEvent(QKeyEvent *event) {
        if (event->key() == Qt::Key_Escape && m_gesture) {
            cancelGesture();
            event->accept();
            return;
        }
        QAbstractScrollArea::keyPressEvent(event);
    }

    void SceneView::leaveEvent(QEvent *event) {
        if (!m_gesture && m_hovered) {
            m_hovered.reset();
            viewport()->unsetCursor();
        }
        QAbstractScrollArea::leaveEvent(event);
    }

    void SceneView::focusOutEvent(QFocusEvent *event) {
        // A dialog or another window takes the keyboard, and the release may never arrive.
        cancelGesture();
        QAbstractScrollArea::focusOutEvent(event);
    }

    TimeAxis SceneView::clamped(TimeAxis axis) const {
        axis.pixelsPerTick = std::clamp(axis.pixelsPerTick, m_minimumTimeScale, m_maximumTimeScale);
        const double visible = viewport()->width() / axis.pixelsPerTick;
        axis.left = std::clamp(axis.left, m_firstTick, std::max(m_firstTick, m_lastTick - visible));
        return axis;
    }

    KeyAxis SceneView::clamped(KeyAxis axis) const {
        axis.pixelsPerKey = std::clamp(axis.pixelsPerKey, m_minimumKeyScale, m_maximumKeyScale);
        const double visible = viewport()->height() / axis.pixelsPerKey;
        const double highest = m_highestKey + 1;
        axis.top = std::clamp(axis.top, std::min(highest, m_lowestKey + visible), highest);
        return axis;
    }

    void SceneView::updateScrollBars() {
        m_updatingScrollBars = true;

        const double width = viewport()->width();
        const double sceneWidth = (m_lastTick - m_firstTick) * m_timeAxis.pixelsPerTick;
        horizontalScrollBar()->setRange(0, int(std::max(0.0, std::ceil(sceneWidth - width))));
        horizontalScrollBar()->setPageStep(int(width));
        horizontalScrollBar()->setSingleStep(int(width * WidthPerNotch));
        horizontalScrollBar()->setValue(
            int(std::lround((m_timeAxis.left - m_firstTick) * m_timeAxis.pixelsPerTick)));

        const double height = viewport()->height();
        const double sceneHeight = (m_highestKey + 1 - m_lowestKey) * m_keyAxis.pixelsPerKey;
        verticalScrollBar()->setRange(0, int(std::max(0.0, std::ceil(sceneHeight - height))));
        verticalScrollBar()->setPageStep(int(height));
        verticalScrollBar()->setSingleStep(int(m_keyAxis.pixelsPerKey));
        verticalScrollBar()->setValue(
            int(std::lround((m_highestKey + 1 - m_keyAxis.top) * m_keyAxis.pixelsPerKey)));

        m_updatingScrollBars = false;
    }

    void SceneView::updateHover(QPointF position) {
        const auto hit = hitAt(position);
        if (hit == m_hovered) {
            return;
        }
        m_hovered = hit;
        if (hit) {
            viewport()->setCursor(hit->cursor);
        } else {
            viewport()->unsetCursor();
        }
    }

    void SceneView::cancelGesture() {
        if (m_gesture) {
            auto gesture = std::move(m_gesture);
            gesture->cancel();
        }
    }

}
