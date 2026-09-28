#include "TimelineRuler.h"

#include <algorithm>
#include <cmath>

#include <QtGui/QPainter>

#include "SceneView.h"

namespace hello::daw {

    namespace {

        // Beat lines are drawn only this far apart at least, in pixels.
        constexpr double MinimumBeatSpacing = 8;

        // Space around a bar number, in pixels.
        constexpr int LabelPadding = 4;

    }

    TimelineRuler::TimelineRuler(SceneView *view, QWidget *parent) : QWidget(parent), m_view(view) {
        connect(view, &SceneView::timeAxisChanged, this, qOverload<>(&QWidget::update));
    }

    TimelineRuler::~TimelineRuler() = default;

    int TimelineRuler::ticksPerBeat() const {
        return m_ticksPerBeat;
    }

    void TimelineRuler::setTicksPerBeat(int ticks) {
        m_ticksPerBeat = std::max(1, ticks);
        update();
    }

    int TimelineRuler::beatsPerBar() const {
        return m_beatsPerBar;
    }

    void TimelineRuler::setBeatsPerBar(int beats) {
        m_beatsPerBar = std::max(1, beats);
        update();
    }

    QList<TimelineRuler::Mark> TimelineRuler::marks() const {
        return m_marks;
    }

    void TimelineRuler::setMarks(const QList<Mark> &marks) {
        m_marks = marks;
        update();
    }

    QColor TimelineRuler::lineColor() const {
        if (m_lineColor.isValid()) {
            return m_lineColor;
        }
        auto color = palette().color(QPalette::WindowText);
        color.setAlphaF(0.6f);
        return color;
    }

    void TimelineRuler::setLineColor(const QColor &color) {
        m_lineColor = color;
        update();
    }

    QColor TimelineRuler::markColor() const {
        return m_markColor.isValid() ? m_markColor : palette().color(QPalette::Link);
    }

    void TimelineRuler::setMarkColor(const QColor &color) {
        m_markColor = color;
        update();
    }

    QSize TimelineRuler::sizeHint() const {
        return {0, fontMetrics().height() * 2 + 2 * LabelPadding};
    }

    int TimelineRuler::labelInterval(double barWidth, double labelWidth) {
        int interval = 1;
        while (barWidth * interval < labelWidth + 2 * LabelPadding && interval < (1 << 20)) {
            interval *= 2;
        }
        return interval;
    }

    void TimelineRuler::paintEvent(QPaintEvent *event) {
        Q_UNUSED(event);
        if (!m_view) {
            return;
        }
        QPainter painter(this);
        const auto &axis = m_view->timeAxis();
        // The ruler starts where the viewport of the view does.
        const double offset =
            m_view->viewport()->mapTo(window(), QPoint()).x() - mapTo(window(), QPoint()).x();
        const auto metrics = fontMetrics();
        const int half = height() / 2;

        const double barTicks = double(m_ticksPerBeat) * m_beatsPerBar;
        const double beatWidth = m_ticksPerBeat * axis.pixelsPerTick;
        const double barWidth = barTicks * axis.pixelsPerTick;
        const int interval = labelInterval(
            barWidth,
            metrics.horizontalAdvance(QString::number(int(axis.toTick(width()) / barTicks) + 1)));

        const double firstTick = std::max(0.0, axis.toTick(-offset));
        const double lastTick = axis.toTick(width() - offset);
        const auto firstBar = qint64(std::floor(firstTick / barTicks));
        const auto lastBar = qint64(std::ceil(lastTick / barTicks));

        painter.setPen(lineColor());
        for (auto bar = firstBar; bar <= lastBar; ++bar) {
            const double barX = offset + axis.toX(double(bar) * barTicks);
            painter.drawLine(QPointF(barX, half), QPointF(barX, height()));
            if (bar % interval == 0) {
                painter.drawText(QRectF(barX + LabelPadding, 0, barWidth * interval, half),
                                 Qt::AlignLeft | Qt::AlignVCenter, QString::number(bar + 1));
            }
            if (beatWidth >= MinimumBeatSpacing) {
                for (int beat = 1; beat < m_beatsPerBar; ++beat) {
                    const double beatX = barX + beat * beatWidth;
                    painter.drawLine(QPointF(beatX, height() - half / 2), QPointF(beatX, height()));
                }
            }
        }
        painter.drawLine(0, height() - 1, width(), height() - 1);

        painter.setPen(markColor());
        for (const auto &mark : m_marks) {
            const double x = offset + axis.toX(mark.tick);
            if (x < -metrics.horizontalAdvance(mark.text) || x > width()) {
                continue;
            }
            painter.drawText(QRectF(x + LabelPadding, half, width(), half),
                             Qt::AlignLeft | Qt::AlignVCenter, mark.text);
        }
    }

}
