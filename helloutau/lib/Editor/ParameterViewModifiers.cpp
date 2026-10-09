#include "ParameterViewModifiers.h"

#include <QtCore/QCoreApplication>

namespace hello::daw {

    const ModifierScheme &ParameterViewModifiers::scheme() {
        static const ModifierScheme scheme(
            "parameterView", "hello::daw::ParameterViewModifiers",
            QT_TRANSLATE_NOOP("hello::daw::ParameterViewModifiers", "Parameter Area"),
            {
                {DragValue, "dragValue",
                 QT_TRANSLATE_NOOP("hello::daw::ParameterViewModifiers", "Drag: Edit"),
                 Qt::NoModifier},
                {LockTime, "lockTime",
                 QT_TRANSLATE_NOOP("hello::daw::ParameterViewModifiers", "Drag: Lock Time"),
                 Qt::ShiftModifier},
                {SnapValue, "snapValue",
                 QT_TRANSLATE_NOOP("hello::daw::ParameterViewModifiers", "Drag: Snap Value"),
                 Qt::ControlModifier},
            },
            {
                {DragScene, {{DragValue, {LockTime, SnapValue}}}},
            });
        return scheme;
    }

}
