#ifndef HELLOUTAU_WIDGETS_TIMELINERULER_H
#define HELLOUTAU_WIDGETS_TIMELINERULER_H

#include <QtCore/QList>
#include <QtCore/QPointer>
#include <QtGui/QColor>
#include <QtWidgets/QWidget>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

namespace hello::daw {

    class SceneView;

    /// The ruler above a scene view: in its top row named sections such as labels and regions,
    /// below them bar numbers and beat lines along its time axis, and marks at given positions,
    /// such as changes of tempo.
    ///
    /// The ruler follows the time axis of its view. Lines are drawn as densely as they stay
    /// apart: beats only when they are far enough apart, and bar numbers at an interval that
    /// keeps them legible.
    class HELLOUTAU_WIDGETS_EXPORT TimelineRuler : public QWidget {
        Q_OBJECT
        Q_PROPERTY(QColor lineColor READ lineColor WRITE setLineColor)
        Q_PROPERTY(QColor markColor READ markColor WRITE setMarkColor)
    public:
        /// A label at a position of the time axis, drawn in the lower half of the ruler.
        struct Mark {
            double tick = 0;
            QString text;
        };

        /// A stretch of the time axis from \a first to \a last ticks, drawn in \a color as a
        /// strip along the bottom edge of the ruler, such as how far the notes there are
        /// rendered.
        struct Span {
            double first = 0;
            double last = 0;
            QColor color;
        };

        /// A named stretch of the time axis from \a first to \a last ticks, drawn in the top row
        /// of the ruler in markColor(): filled for a label, outlined for a region. Labels are
        /// drawn over regions.
        struct Section {
            double first = 0;
            double last = 0;
            QString text;
            bool filled = false;
        };

        explicit TimelineRuler(SceneView *view, QWidget *parent = nullptr);
        ~TimelineRuler();

        /// The length of a beat and the number of beats in a bar. UST has no time signature,
        /// and UTAU shows 4/4 of 480 ticks per beat, the defaults.
        int ticksPerBeat() const;
        void setTicksPerBeat(int ticks);
        int beatsPerBar() const;
        void setBeatsPerBar(int beats);

        QList<Mark> marks() const;
        void setMarks(const QList<Mark> &marks);

        QList<Span> spans() const;
        void setSpans(const QList<Span> &spans);

        QList<Section> sections() const;
        void setSections(const QList<Section> &sections);

        /// The color of the lines and bar numbers, by default that of text with some
        /// transparency.
        QColor lineColor() const;
        void setLineColor(const QColor &color);

        /// The color of the marks, by default that of links.
        QColor markColor() const;
        void setMarkColor(const QColor &color);

        QSize sizeHint() const override;

        /// Returns the interval in bars between two numbered bars, so that numbers \a barWidth
        /// pixels apart do not crowd: 1, 2, 4, 8 and so on.
        static int labelInterval(double barWidth, double labelWidth);

    Q_SIGNALS:
        /// The left button was pressed at \a tick, or moved there while held, with
        /// \a modifiers: a request to move the playhead, which the owner of the ruler decides.
        void positionPressed(double tick, Qt::KeyboardModifiers modifiers);

        /// Mark \a index of marks() was double-clicked on its text.
        void markDoubleClicked(int index);

        /// Section \a index of sections() was double-clicked.
        void sectionDoubleClicked(int index);

        /// The context menu was asked for at \a tick, to be shown at \a globalPosition.
        void menuRequested(double tick, const QPoint &globalPosition);

    protected:
        void paintEvent(QPaintEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void mouseDoubleClickEvent(QMouseEvent *event) override;
        void contextMenuEvent(QContextMenuEvent *event) override;

    private:
        QPointer<SceneView> m_view;
        int m_ticksPerBeat = 480;
        int m_beatsPerBar = 4;
        QList<Mark> m_marks;
        QList<Span> m_spans;
        QList<Section> m_sections;
        QColor m_lineColor;
        QColor m_markColor;

        // Where the ruler starts in the time axis of the view, as it is painted
        double offset() const;

        // The height of the top row, of the sections; the bar numbers and the marks share the
        // rest in halves.
        int sectionRowHeight() const;

        // The index of the mark whose text is at position, or -1
        int markAt(const QPointF &position) const;

        // The index of the section at position, a label before a region, or -1
        int sectionAt(const QPointF &position) const;
    };

}

#endif // HELLOUTAU_WIDGETS_TIMELINERULER_H
