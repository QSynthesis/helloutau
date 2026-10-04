#ifndef HELLOUTAU_WIDGETS_SCROLLAREABASE_H
#define HELLOUTAU_WIDGETS_SCROLLAREABASE_H

#include <QtWidgets/QAbstractScrollArea>

#include <QtCore/QList>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

namespace hello::daw {

    /// Base for scroll areas that must give Alt-wheel events to their specialized wheel handler
    /// before Qt's platform-specific horizontal scrolling can consume them.
    class HELLOUTAU_WIDGETS_EXPORT ScrollAreaBase : public QAbstractScrollArea {
    public:
        /// Sets the modifier combinations that Alt-wheel events are allowed to reach the view.
        void setWheelModifierCombinations(const QList<Qt::KeyboardModifiers> &combinations);

    protected:
        using QAbstractScrollArea::QAbstractScrollArea;

        bool viewportEvent(QEvent *event) override;

    private:
        QList<Qt::KeyboardModifiers> m_wheelModifierCombinations;
    };

}

#endif // HELLOUTAU_WIDGETS_SCROLLAREABASE_H
