#ifndef HELLOUTAU_WIDGETS_PIANOKEYBOARD_H
#define HELLOUTAU_WIDGETS_PIANOKEYBOARD_H

#include <QtCore/QPointer>
#include <QtGui/QColor>
#include <QtWidgets/QWidget>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

class QMouseEvent;
class QEvent;

namespace hello::daw {

    class SceneView;

    /// The keyboard beside a scene view: one key per row of its key axis, the black keys
    /// shorter, and the name of each C.
    class HELLOUTAU_WIDGETS_EXPORT PianoKeyboard : public QWidget {
        Q_OBJECT
        Q_PROPERTY(QColor whiteKeyColor READ whiteKeyColor WRITE setWhiteKeyColor)
        Q_PROPERTY(QColor blackKeyColor READ blackKeyColor WRITE setBlackKeyColor)
        Q_PROPERTY(QColor lineColor READ lineColor WRITE setLineColor)
    public:
        explicit PianoKeyboard(SceneView *view, QWidget *parent = nullptr);
        ~PianoKeyboard();

        /// Returns whether \a key, a MIDI note number, is a black key.
        static bool isBlackKey(int key);

        /// Returns the name of \a key, where 60 is C4, as UTAU names it.
        static QString keyName(int key);

        QColor whiteKeyColor() const;
        void setWhiteKeyColor(const QColor &color);
        QColor blackKeyColor() const;
        void setBlackKeyColor(const QColor &color);
        QColor lineColor() const;
        void setLineColor(const QColor &color);

        QSize sizeHint() const override;

    Q_SIGNALS:
        /// A piano key was pressed. The value is a MIDI note number.
        void keyPressed(int key);

    protected:
        void paintEvent(QPaintEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void leaveEvent(QEvent *event) override;

    private:
        QPointer<SceneView> m_view;
        int m_hoveredKey = -1;
        QColor m_whiteKeyColor;
        QColor m_blackKeyColor;
        QColor m_lineColor;
    };

}

#endif // HELLOUTAU_WIDGETS_PIANOKEYBOARD_H
