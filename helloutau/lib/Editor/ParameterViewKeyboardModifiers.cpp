#include "ParameterViewKeyboardModifiers.h"

#include <QtCore/QCoreApplication>

namespace hello::daw {

    const KeyboardModifierScheme &ParameterViewKeyboardModifiers::scheme() {
        static const KeyboardModifierScheme scheme(
            "parameterView", "hello::daw::ParameterViewKeyboardModifiers",
            QT_TRANSLATE_NOOP("hello::daw::ParameterViewKeyboardModifiers", "Parameter Area"),
            {
                {DragValue, "dragValue",
                 QT_TRANSLATE_NOOP("hello::daw::ParameterViewKeyboardModifiers", "Drag: Edit"),
                 Qt::NoModifier},
                {LockTime, "lockTime",
                 QT_TRANSLATE_NOOP("hello::daw::ParameterViewKeyboardModifiers", "Drag: Lock Time"),
                 Qt::ShiftModifier},
                {SnapValue, "snapValue",
                 QT_TRANSLATE_NOOP("hello::daw::ParameterViewKeyboardModifiers",
                                   "Drag: Snap Value"),
                 Qt::ControlModifier},
            },
            {
                {DragScene, {{DragValue, {LockTime, SnapValue}}}},
            });
        return scheme;
    }

}
