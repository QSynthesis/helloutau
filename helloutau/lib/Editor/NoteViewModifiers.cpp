#include "NoteViewModifiers.h"

#include <QtCore/QCoreApplication>

namespace hello::daw {

    namespace {

        // The conflict sets of the roles
        enum ConflictSet : quint32 {
            WheelSet = 1 << 0,
            DragZoomSet = 1 << 1,
        };

    }

    const ModifierScheme &NoteViewModifiers::scheme() {
        static const ModifierScheme scheme(
            "noteView", "hello::daw::NoteViewModifiers",
            QT_TRANSLATE_NOOP("hello::daw::NoteViewModifiers", "Note Area"),
            {
                {HorizontalScroll, "horizontalScroll",
                 QT_TRANSLATE_NOOP("hello::daw::NoteViewModifiers", "Horizontal Scroll"),
                 ModifierScheme::Exact, WheelSet, Qt::ShiftModifier},
                {TimeZoom, "timeZoom",
                 QT_TRANSLATE_NOOP("hello::daw::NoteViewModifiers", "Time Zoom"),
                 ModifierScheme::Exact, WheelSet, Qt::ControlModifier},
                {KeyZoom, "keyZoom", QT_TRANSLATE_NOOP("hello::daw::NoteViewModifiers", "Key Zoom"),
                 ModifierScheme::Exact, WheelSet, Qt::ControlModifier | Qt::ShiftModifier},
                {DragZoom, "dragZoom",
                 QT_TRANSLATE_NOOP("hello::daw::NoteViewModifiers", "Drag Zoom"),
                 ModifierScheme::Contains, DragZoomSet, Qt::ControlModifier | Qt::AltModifier},
                {DragZoomAxisLock, "dragZoomAxisLock",
                 QT_TRANSLATE_NOOP("hello::daw::NoteViewModifiers", "Drag Zoom Axis Lock"),
                 ModifierScheme::Contains, DragZoomSet, Qt::ShiftModifier},
                {DisableNoteSnap, "disableNoteSnap",
                 QT_TRANSLATE_NOOP("hello::daw::NoteViewModifiers", "Disable Note Snap"),
                 ModifierScheme::Contains, 0, Qt::AltModifier},
            });
        return scheme;
    }

}
