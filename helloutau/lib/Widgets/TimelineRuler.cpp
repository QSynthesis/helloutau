#include "TimelineRuler.h"

#include <algorithm>
#include <cmath>

#include <QtGui/QContextMenuEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPen>

#include "SceneView.h"

namespace hello::daw {

    namespace {

        // Beat lines are drawn only this far apart at least, in pixels.
        constexpr double MinimumBeatSpacing = 8;

        // Space around a bar number, in pixels.
        constexpr int LabelPadding = 4;

        // The height of the strip of the spans, in pixels.
        constexpr int SpanHeight = 3;

    }

    TimelineRuler::TimelineRuler(SceneView *view, QWidget *parent) : QWidget(parent), m_view(view) {
        setMouseTracking(true);
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

    QList<TimelineRuler::Span> TimelineRuler::spans() const {
        return m_spans;
    }

    void TimelineRuler::setSpans(const QList<Span> &spans) {
        m_spans = spans;
        update();
    }

    QList<TimelineRuler::Section> TimelineRuler::sections() const {
        return m_sections;
    }

    void TimelineRuler::setSections(const QList<Section> &sections) {
        m_sections = sections;
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
        return {0, fontMetrics().height() * 4 + 4 * LabelPadding};
    }

    int TimelineRuler::sectionRowHeight() const {
        return height() / 4;
    }

    int TimelineRuler::labelInterval(double barWidth, double labelWidth) {
        int interval = 1;
        while (barWidth * interval < labelWidth + 2 * LabelPadding && interval < (1 << 20)) {
            interval *= 2;
        }
        return interval;
    }

    void TimelineRuler::mousePressEvent(QMouseEvent *event) {
        if (event->button() != Qt::LeftButton || !m_view) {
            QWidget::mousePressEvent(event);
            return;
        }
        if (const int section = sectionAt(event->position()); section >= 0) {
            Q_EMIT sectionClicked(section);
            return;
        }
        mouseMoveEvent(event);
    }

    double TimelineRuler::offset() const {
        return m_view->viewport()->mapTo(window(), QPoint()).x() - mapTo(window(), QPoint()).x();
    }

    int TimelineRuler::markAt(const QPointF &position) const {
        const int top = sectionRowHeight();
        if (!m_view || position.y() < 2 * top + (height() - 2 * top) / 2) {
            return -1;
        }
        const auto metrics = fontMetrics();
        for (int i = 0; i < m_marks.size(); ++i) {
            const double left = offset() + m_view->timeAxis().toX(m_marks[i].tick) + LabelPadding;
            if (position.x() >= left &&
                position.x() <= left + metrics.horizontalAdvance(m_marks[i].text)) {
                return i;
            }
        }
        return -1;
    }

    int TimelineRuler::sectionAt(const QPointF &position) const {
        if (!m_view || position.y() < 0 || position.y() >= 2 * sectionRowHeight()) {
            return -1;
        }
        const bool labelRow = position.y() < sectionRowHeight();
        const double tick = m_view->timeAxis().toTick(position.x() - offset());
        for (int i = 0; i < m_sections.size(); ++i) {
            const auto &section = m_sections[i];
            if (section.filled == labelRow && tick >= section.first && tick < section.last) {
                return i;
            }
        }
        return -1;
    }

    void TimelineRuler::mouseDoubleClickEvent(QMouseEvent *event) {
        if (event->button() == Qt::LeftButton) {
            if (const int mark = markAt(event->position()); mark >= 0) {
                Q_EMIT markDoubleClicked(mark);
                return;
            }
            if (const int section = sectionAt(event->position()); section >= 0) {
                Q_EMIT sectionDoubleClicked(section);
                return;
            }
        }
        QWidget::mouseDoubleClickEvent(event);
    }

    void TimelineRuler::contextMenuEvent(QContextMenuEvent *event) {
        if (!m_view) {
            QWidget::contextMenuEvent(event);
            return;
        }
        Q_EMIT menuRequested(m_view->timeAxis().toTick(event->pos().x() - offset()),
                             event->globalPos());
    }

    void TimelineRuler::mouseMoveEvent(QMouseEvent *event) {
        if (!m_view) {
            QWidget::mouseMoveEvent(event);
            return;
        }
        updateSectionHover(event->position());
        if (!(event->buttons() & Qt::LeftButton)) {
            QWidget::mouseMoveEvent(event);
            return;
        }
        // As painted: the ruler starts where the viewport of the view does.
        Q_EMIT positionPressed(m_view->timeAxis().toTick(event->position().x() - offset()),
                               event->modifiers());
    }

    void TimelineRuler::updateSectionHover(const QPointF &position) {
        const int section = sectionAt(position);
        if (section != m_hoveredSection) {
            m_hoveredSection = section;
            update();
        }
    }

    void TimelineRuler::leaveEvent(QEvent *event) {
        m_hoveredSection = -1;
        update();
        QWidget::leaveEvent(event);
    }

    void TimelineRuler::paintEvent(QPaintEvent *event) {
        Q_UNUSED(event);
        if (!m_view) {
            return;
        }
        QPainter painter(this);
        const auto &axis = m_view->timeAxis();
        // The ruler starts where the viewport of the view does.
        const double offset = this->offset();
        const auto metrics = fontMetrics();
        // Labels and regions in separate rows, then the bar numbers and marks in halves of the
        // remaining area.
        const int top = sectionRowHeight();
        const int timelineTop = 2 * top;
        const int half = (height() - timelineTop) / 2;

        const double barTicks = double(m_ticksPerBeat) * m_beatsPerBar;
        const double barWidth = barTicks * axis.pixelsPerTick;
        const int interval = labelInterval(
            barWidth,
            metrics.horizontalAdvance(QString::number(int(axis.toTick(width()) / barTicks) + 1)));

        const double firstTick = std::max(0.0, axis.toTick(-offset));
        const double lastTick = axis.toTick(width() - offset);
        const auto firstBar = qint64(std::floor(firstTick / barTicks));
        const auto lastBar = qint64(std::ceil(lastTick / barTicks));

        const int subdivisions[] = {m_ticksPerBeat,
                                    std::max(1, m_ticksPerBeat / 2),
                                    std::max(1, m_ticksPerBeat / 4),
                                    std::max(1, m_ticksPerBeat / 8)};
        int step = subdivisions[0];
        bool drawSubdivisions = false;
        for (const int candidate : subdivisions) {
            if (candidate * axis.pixelsPerTick >= MinimumBeatSpacing) {
                step = candidate;
                drawSubdivisions = true;
            }
        }
        auto subdivisionColor = markColor();
        subdivisionColor.setAlphaF(subdivisionColor.alphaF() * 0.55);

        painter.setPen(lineColor());
        for (auto bar = firstBar; bar <= lastBar; ++bar) {
            const double barX = offset + axis.toX(double(bar) * barTicks);
            painter.drawLine(QPointF(barX, timelineTop + half), QPointF(barX, height()));
            if (bar % interval == 0) {
                painter.drawText(
                    QRectF(barX + LabelPadding, timelineTop, barWidth * interval, half),
                    Qt::AlignLeft | Qt::AlignVCenter, QString::number(bar + 1));
            }
            for (int tick = step; tick < barTicks; tick += step) {
                if (!drawSubdivisions && step == subdivisions[0]) {
                    break;
                }
                const bool beat = tick % m_ticksPerBeat == 0;
                painter.setPen(beat ? markColor() : subdivisionColor);
                const double tickX = barX + tick * axis.pixelsPerTick;
                painter.drawLine(QPointF(tickX, height() - half / 2), QPointF(tickX, height()));
            }
        }
        // The spans above the bottom line
        for (const auto &span : std::as_const(m_spans)) {
            const double left = std::max(0.0, offset + axis.toX(span.first));
            const double right = std::min(double(width()), offset + axis.toX(span.last));
            if (right > left) {
                painter.fillRect(QRectF(left, height() - 1 - SpanHeight, right - left, SpanHeight),
                                 span.color);
            }
        }
        painter.drawLine(0, height() - 1, width(), height() - 1);

        painter.setPen(markColor());
        for (const auto &mark : m_marks) {
            const double x = offset + axis.toX(mark.tick);
            if (x < -metrics.horizontalAdvance(mark.text) || x > width()) {
                continue;
            }
            painter.drawText(QRectF(x + LabelPadding, timelineTop + half, width(), half),
                             Qt::AlignLeft | Qt::AlignVCenter, mark.text);
        }

        // Regions in the lower section row, labels in the upper section row.
        auto fill = markColor();
        fill.setAlphaF(fill.alphaF() * 0.25f);
        for (int index = 0; index < m_sections.size(); ++index) {
            const auto &section = m_sections.at(index);
            const double left = offset + axis.toX(section.first);
            const double right = offset + axis.toX(section.last);
            if (right < 0 || left > width()) {
                continue;
            }
            const double rowTop = section.filled ? 0 : top;
            const QRectF box(left + 0.5, rowTop + 1.5, std::max(1.0, right - left - 1), top - 3);
            QPen pen(markColor());
            if (section.selected || index == m_hoveredSection) {
                pen.setColor(pen.color().lighter(130));
                pen.setWidthF(2.0);
            }
            painter.setPen(pen);
            painter.setBrush(section.filled ? QBrush(fill) : QBrush(Qt::NoBrush));
            painter.drawRect(box);
            painter.drawText(box.adjusted(LabelPadding, 0, -LabelPadding, 0),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             metrics.elidedText(section.text, Qt::ElideRight,
                                                int(box.width()) - 2 * LabelPadding));
        }
    }

}
