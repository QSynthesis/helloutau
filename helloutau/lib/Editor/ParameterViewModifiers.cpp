#include "ParameterViewModifiers.h"

#include <QtCore/QCoreApplication>

namespace hello::daw {

    namespace {

        // The conflict sets of the roles
        enum ConflictSet : quint32 {
            DragSet = 1 << 0,
        };

    }

    const ModifierScheme &ParameterViewModifiers::scheme() {
        static const ModifierScheme scheme(
            "parameterView", "hello::daw::ParameterViewModifiers",
            QT_TRANSLATE_NOOP("hello::daw::ParameterViewModifiers", "Parameter Area"),
            {
                {LockTime, "lockTime",
                 QT_TRANSLATE_NOOP("hello::daw::ParameterViewModifiers", "Lock Time"),
                 ModifierScheme::Contains, DragSet, Qt::ShiftModifier},
                {SnapValue, "snapValue",
                 QT_TRANSLATE_NOOP("hello::daw::ParameterViewModifiers", "Snap Value"),
                 ModifierScheme::Contains, DragSet, Qt::ControlModifier},
            });
        return scheme;
    }

}
