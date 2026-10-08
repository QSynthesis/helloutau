#include "ParameterViewModifiers.h"

#include <QtCore/QCoreApplication>

namespace hello::daw {

    const ModifierScheme &ParameterViewModifiers::scheme() {
        static const ModifierScheme scheme(
            "parameterView", "hello::daw::ParameterViewModifiers",
            QT_TRANSLATE_NOOP("hello::daw::ParameterViewModifiers", "Parameter Area"),
            {
                {LockTime, "lockTime",
                 QT_TRANSLATE_NOOP("hello::daw::ParameterViewModifiers", "Lock Time"),
                 ModifierScheme::Contains, 0, Qt::ShiftModifier},
                {SnapValue, "snapValue",
                 QT_TRANSLATE_NOOP("hello::daw::ParameterViewModifiers", "Snap Value"),
                 ModifierScheme::Contains, 0, Qt::ControlModifier},
            });
        return scheme;
    }

}
