#include "PianoKeyboard.h"

#include <cmath>

#include <QtGui/QPainter>
#include <QtGui/QMouseEvent>

#include "SceneView.h"

namespace hello::daw {

    namespace {

        // A black key covers this part of the width of the keyboard.
        constexpr double BlackKeyLength = 0.6;

        constexpr int LabelPadding = 4;

        constexpr int DefaultWidth = 80;

    }

    PianoKeyboard::PianoKeyboard(SceneView *view, QWidget *parent) : QWidget(parent), m_view(view) {
        connect(view, &SceneView::keyAxisChanged, this, qOverload<>(&QWidget::update));
    }

    PianoKeyboard::~PianoKeyboard() = default;

    bool PianoKeyboard::isBlackKey(int key) {
        switch ((key % 12 + 12) % 12) {
            case 1:
            case 3:
            case 6:
            case 8:
            case 10:
                return true;
            default:
                return false;
        }
    }

    QString PianoKeyboard::keyName(int key) {
        static const char *const names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                            "F#", "G",  "G#", "A",  "A#", "B"};
        const int octave = int(std::floor(key / 12.0)) - 1;
        return QString::fromLatin1(names[(key % 12 + 12) % 12]) + QString::number(octave);
    }

    QColor PianoKeyboard::whiteKeyColor() const {
        return m_whiteKeyColor.isValid() ? m_whiteKeyColor : QColor(Qt::white);
    }

    void PianoKeyboard::setWhiteKeyColor(const QColor &color) {
        m_whiteKeyColor = color;
        update();
    }

    QColor PianoKeyboard::blackKeyColor() const {
        return m_blackKeyColor.isValid() ? m_blackKeyColor : QColor(0x40, 0x40, 0x40);
    }

    void PianoKeyboard::setBlackKeyColor(const QColor &color) {
        m_blackKeyColor = color;
        update();
    }

    QColor PianoKeyboard::lineColor() const {
        return m_lineColor.isValid() ? m_lineColor : QColor(0xB0, 0xB0, 0xB0);
    }

    void PianoKeyboard::setLineColor(const QColor &color) {
        m_lineColor = color;
        update();
    }

    QSize PianoKeyboard::sizeHint() const {
        return {DefaultWidth, 0};
    }

    void PianoKeyboard::paintEvent(QPaintEvent *event) {
        Q_UNUSED(event);
        if (!m_view) {
            return;
        }
        QPainter painter(this);
        painter.fillRect(rect(), whiteKeyColor());

        const auto &axis = m_view->keyAxis();
        // The keyboard starts where the viewport of the view does.
        const double offset =
            m_view->viewport()->mapTo(window(), QPoint()).y() - mapTo(window(), QPoint()).y();
        const int highest = axis.keyAt(-offset);
        const int lowest = axis.keyAt(height() - offset);

        for (int key = lowest; key <= highest; ++key) {
            const double top = offset + axis.toY(key + 1);
            const double bottom = offset + axis.toY(key);
            if (isBlackKey(key)) {
                painter.fillRect(QRectF(0, top, width() * BlackKeyLength, bottom - top),
                                 blackKeyColor());
            }
            // White keys are separated where no black key lies between them, below C and F.
            const int degree = (key % 12 + 12) % 12;
            if (degree == 0 || degree == 5) {
                painter.setPen(lineColor());
                painter.drawLine(QPointF(0, bottom), QPointF(width(), bottom));
            }
            if (degree == 0) {
                painter.setPen(palette().color(QPalette::Text));
                painter.drawText(QRectF(0, top, width() - LabelPadding, bottom - top),
                                 Qt::AlignRight | Qt::AlignVCenter, keyName(key));
            }
        }
        painter.setPen(lineColor());
        painter.drawLine(width() - 1, 0, width() - 1, height());
    }

    void PianoKeyboard::mousePressEvent(QMouseEvent *event) {
        if (!m_view || event->button() != Qt::LeftButton) {
            QWidget::mousePressEvent(event);
            return;
        }
        const auto &axis = m_view->keyAxis();
        const double offset =
            m_view->viewport()->mapTo(window(), QPoint()).y() - mapTo(window(), QPoint()).y();
        const int key = axis.keyAt(event->position().y() - offset);
        if (key >= 0 && key <= 127) {
            Q_EMIT keyPressed(key);
        }
        event->accept();
    }

}
