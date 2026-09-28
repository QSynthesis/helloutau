#ifndef HELLOUTAU_WIDGETS_TIMELINERULER_H
#define HELLOUTAU_WIDGETS_TIMELINERULER_H

#include <QtCore/QList>
#include <QtCore/QPointer>
#include <QtGui/QColor>
#include <QtWidgets/QWidget>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

namespace hello::daw {

    class SceneView;

    /// The ruler above a scene view: bar numbers and beat lines along its time axis, and marks
    /// at given positions, such as changes of tempo.
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

    protected:
        void paintEvent(QPaintEvent *event) override;

    private:
        QPointer<SceneView> m_view;
        int m_ticksPerBeat = 480;
        int m_beatsPerBar = 4;
        QList<Mark> m_marks;
        QColor m_lineColor;
        QColor m_markColor;
    };

}

#endif // HELLOUTAU_WIDGETS_TIMELINERULER_H
