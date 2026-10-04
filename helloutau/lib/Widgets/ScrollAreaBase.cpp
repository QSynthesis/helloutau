#include <helloutau/Widgets/ScrollAreaBase.h>

#include <QtGui/QWheelEvent>

namespace hello::daw {

    void ScrollAreaBase::setWheelModifierCombinations(
        const QList<Qt::KeyboardModifiers> &combinations) {
        m_wheelModifierCombinations = combinations;
    }

    bool ScrollAreaBase::viewportEvent(QEvent *event) {
        if (event->type() == QEvent::Wheel) {
            auto *wheel = static_cast<QWheelEvent *>(event);
            if (wheel->modifiers() & Qt::AltModifier) {
                constexpr auto standardModifiers = Qt::ControlModifier | Qt::AltModifier |
                                                   Qt::ShiftModifier | Qt::MetaModifier;
                const auto modifiers = wheel->modifiers() & standardModifiers;
                if (!m_wheelModifierCombinations.contains(modifiers)) {
                    wheel->accept();
                    return true;
                }
                wheelEvent(wheel);
                return true;
            }
        }
        return QAbstractScrollArea::viewportEvent(event);
    }

}
