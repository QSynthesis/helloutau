#ifndef HELLOUTAU_WIDGETS_SCENEVIEW_H
#define HELLOUTAU_WIDGETS_SCENEVIEW_H

#include <memory>
#include <optional>
#include <vector>

#include <QtCore/QPointF>
#include <QtCore/QTimer>
#include <QtWidgets/QAbstractScrollArea>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>
#include <helloutau/Widgets/SceneAxis.h>
#include <helloutau/Widgets/SceneLayer.h>

namespace hello::daw {

    /// A view of a scene laid out in ticks across and keys up and down, such as a piano roll,
    /// drawn by layers and scrolled and zoomed by the axes.
    ///
    /// The view keeps no objects for what it shows: each layer draws the visible part from its
    /// own data on every paint, and reports hits at a position by values. The view dispatches
    /// the pointer: hovering shows the cursor of the topmost hit, a press lets the layer of the
    /// hit start a gesture, which then receives the moves and the release, and Escape cancels
    /// it. See the section on the piano roll in docs/Widgets.md.
    ///
    /// The wheel scrolls up and down; with Shift it scrolls across, with Ctrl it zooms the time
    /// axis and with Ctrl and Shift the key axis, keeping the position under the pointer in
    /// place.
    class HELLOUTAU_WIDGETS_EXPORT SceneView : public QAbstractScrollArea {
        Q_OBJECT
    public:
        explicit SceneView(QWidget *parent = nullptr);
        ~SceneView();

        const TimeAxis &timeAxis() const;

        /// Sets the time axis, with its left edge moved into the range of the scene if needed.
        void setTimeAxis(const TimeAxis &axis);

        const KeyAxis &keyAxis() const;

        /// Sets the modifier combinations used by the wheel for horizontal scrolling and the
        /// two zoom axes. The default combinations are Shift, Ctrl and Ctrl+Shift.
        void setWheelModifiers(Qt::KeyboardModifiers horizontalScroll,
                               Qt::KeyboardModifiers timeZoom,
                               Qt::KeyboardModifiers keyZoom);

        /// Sets the key axis, with its top edge moved into the range of the scene if needed.
        void setKeyAxis(const KeyAxis &axis);

        /// The extent of the scene that can be scrolled to: ticks from \a first to \a last and
        /// keys from \a lowest to \a highest, both included.
        void setTickRange(double first, double last);
        void setKeyRange(int lowest, int highest);

        /// The limits of zooming, as the width of a tick and the height of a key.
        void setTimeScaleRange(double minimum, double maximum);
        void setKeyScaleRange(double minimum, double maximum);

        /// Multiplies the scale of an axis by \a factor, within its limits, keeping the scene
        /// position at the view coordinate \a anchor in place.
        void zoomTime(double factor, double anchor);
        void zoomKeys(double factor, double anchor);

        /// Adds \a layer on top of the others. The view owns it.
        SceneLayer *addLayer(std::unique_ptr<SceneLayer> layer);

        int layerCount() const;
        SceneLayer *layer(int index) const;

        /// Returns the topmost hit at \a position, in view coordinates.
        std::optional<SceneHit> hitAt(QPointF position) const;

        /// The hit under the pointer, if any.
        std::optional<SceneHit> hoveredHit() const;

        /// Whether a gesture is in progress.
        bool hasGesture() const;

    Q_SIGNALS:
        void timeAxisChanged();
        void keyAxisChanged();

    protected:
        void paintEvent(QPaintEvent *event) override;
        void resizeEvent(QResizeEvent *event) override;
        void scrollContentsBy(int dx, int dy) override;
        void wheelEvent(QWheelEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;
        void mouseDoubleClickEvent(QMouseEvent *event) override;
        void keyPressEvent(QKeyEvent *event) override;
        void leaveEvent(QEvent *event) override;
        void focusOutEvent(QFocusEvent *event) override;
        bool eventFilter(QObject *watched, QEvent *event) override;

    private:
        TimeAxis m_timeAxis;
        KeyAxis m_keyAxis;
        double m_firstTick = 0;
        double m_lastTick = 480 * 4 * 32;
        int m_lowestKey = 0;
        int m_highestKey = 127;
        double m_minimumTimeScale = 0.02;
        double m_maximumTimeScale = 2;
        double m_minimumKeyScale = 8;
        double m_maximumKeyScale = 64;

        std::vector<std::unique_ptr<SceneLayer>> m_layers;
        std::optional<SceneHit> m_hovered;
        std::unique_ptr<SceneGesture> m_gesture;
        bool m_updatingScrollBars = false;
        QPointF m_pointerPosition;
        Qt::KeyboardModifiers m_pointerModifiers = Qt::NoModifier;
        Qt::KeyboardModifiers m_horizontalScrollModifiers = Qt::ShiftModifier;
        Qt::KeyboardModifiers m_timeZoomModifiers = Qt::ControlModifier;
        Qt::KeyboardModifiers m_keyZoomModifiers = Qt::ControlModifier | Qt::ShiftModifier;
        QTimer m_autoScrollTimer;
        bool m_suppressContextMenu = false;
        bool m_contextFilterInstalled = false;

        TimeAxis clamped(TimeAxis axis) const;
        KeyAxis clamped(KeyAxis axis) const;
        void updateScrollBars();
        void updateHover(QPointF position);
        void autoScroll();
        void cancelGesture();
        void releaseMouseIfGrabbed();
    };

}

#endif // HELLOUTAU_WIDGETS_SCENEVIEW_H
